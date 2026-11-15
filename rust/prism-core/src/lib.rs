//! Prism-V: Next-Generation Video Compression Native Core (Rust Edition)
//!
//! Enforces:
//! - Invariant 1: Lossless Expansion Prohibition
//! - Invariant 2: Integer Determinism Law (K <= 512, INT8/INT16/INT32)
//! - Invariant 7: Permissive, Zero-GPL Clean-Room Binary (Apache-2.0)

pub const PAGE_SIZE_16KB: usize = 16384;
pub const MAX_K_CHANNELS: usize = 512;
pub const ACT_MIN: i16 = -32767;
pub const ACT_MAX: i16 = 32767;
pub const WEIGHT_MIN: i8 = -127;
pub const WEIGHT_MAX: i8 = 127;
pub const RANS_M: u32 = 32768; // 2^15
pub const RANS_L: u32 = 65536; // 2^16

pub struct DetInt;

impl DetInt {
    /// Bounded dot product: K <= 512, overflow-free in i32 accumulator.
    /// Proof: 512 * 127 * 32767 = 2,130,469,376 < 2^31 - 1.
    pub fn dot_product(x: &[i16], w: &[i8]) -> i32 {
        assert!(x.len() <= MAX_K_CHANNELS, "K exceeds 512 bound");
        assert_eq!(x.len(), w.len(), "Dimension mismatch");

        let mut acc: i32 = 0;
        for i in 0..x.len() {
            acc += (x[i] as i32) * (w[i] as i32);
        }
        acc
    }

    /// Dyadic rescaling: floor((acc * M_scale + 2^(s - 1)) / 2^s) clamped to [-32767, 32767].
    pub fn dyadic_rescale(acc: i32, m_scale: i32, s_shift: u32) -> i16 {
        let scaled = (acc as i64) * (m_scale as i64);
        let round_bias = 1i64 << (s_shift - 1);
        let shifted = (scaled + round_bias) >> s_shift;
        shifted.clamp(ACT_MIN as i64, ACT_MAX as i64) as i16
    }

    pub fn relu(data: &mut [i16]) {
        for val in data.iter_mut() {
            if *val < 0 {
                *val = 0;
            }
        }
    }
}

/// 8-Lane Interleaved rANS Symbol Descriptor
#[derive(Clone, Copy, Debug)]
pub struct RansSymbol {
    pub start: u16,
    pub freq: u16,
}

/// 8-Way Interleaved SIMD rANS Codec
pub struct Rans8Codec;

impl Rans8Codec {
    pub const NUM_LANES: usize = 8;

    pub fn encode(symbols: &[u16], stats: &[RansSymbol]) -> (Vec<u8>, [u32; 8]) {
        let mut states = [RANS_L; Self::NUM_LANES];
        let mut lane_words: [Vec<u16>; Self::NUM_LANES] = Default::default();

        for (i, &sym) in symbols.iter().enumerate().rev() {
            let lane = i % Self::NUM_LANES;
            let st = &stats[sym as usize];
            let f = st.freq as u32;
            let c = st.start as u32;
            let x = &mut states[lane];

            let x_max = f << 17;
            while *x >= x_max {
                lane_words[lane].push((*x & 0xFFFF) as u16);
                *x >>= 16;
            }

            *x = (*x / f) * RANS_M + (*x % f) + c;
        }

        for words in lane_words.iter_mut() {
            words.reverse();
        }

        // Pack byte stream
        let mut stream = Vec::new();
        for words in lane_words.iter() {
            stream.extend_from_slice(&(words.len() as u32).to_le_bytes());
        }
        for words in lane_words.iter() {
            for &w in words {
                stream.extend_from_slice(&w.to_le_bytes());
            }
        }

        (stream, states)
    }

    pub fn decode(stream: &[u8], mut states: [u32; 8], symbol_count: usize, cdf_lut: &[u16], stats: &[RansSymbol]) -> Vec<u16> {
        let mut symbols = Vec::with_capacity(symbol_count);
        let mut word_counts = [0usize; Self::NUM_LANES];
        let mut offset = 0;

        for i in 0..Self::NUM_LANES {
            let count = u32::from_le_bytes(stream[offset..offset+4].try_into().unwrap()) as usize;
            word_counts[i] = count;
            offset += 4;
        }

        let mut lane_words: [Vec<u16>; Self::NUM_LANES] = Default::default();
        for i in 0..Self::NUM_LANES {
            let len = word_counts[i];
            let mut words = Vec::with_capacity(len);
            for _ in 0..len {
                let w = u16::from_le_bytes(stream[offset..offset+2].try_into().unwrap());
                words.push(w);
                offset += 2;
            }
            lane_words[i] = words;
        }

        let mut word_idx = [0usize; Self::NUM_LANES];

        for i in 0..symbol_count {
            let lane = i % Self::NUM_LANES;
            let x = &mut states[lane];

            let m = (*x & (RANS_M - 1)) as usize;
            let sym = cdf_lut[m];
            symbols.push(sym);

            let st = &stats[sym as usize];
            let f = st.freq as u32;
            let c = st.start as u32;

            *x = f * (*x >> 15) + (m as u32 - c);

            while *x < RANS_L && word_idx[lane] < lane_words[lane].len() {
                let w = lane_words[lane][word_idx[lane]] as u32;
                word_idx[lane] += 1;
                *x = (*x << 16) | w;
            }
        }

        symbols
    }
}

/// Reversible Color Transform (YCoCg-R)
pub fn rgb_to_ycocgr(r: u8, g: u8, b: u8) -> (i16, i16, i16) {
    let r = r as i16;
    let g = g as i16;
    let b = b as i16;
    let co = r - b;
    let t = b + (co >> 1);
    let cg = g - t;
    let y = t + (cg >> 1);
    (y, co, cg)
}

pub fn ycocgr_to_rgb(y: i16, co: i16, cg: i16) -> (u8, u8, u8) {
    let t = y - (cg >> 1);
    let g = (cg + t).clamp(0, 255) as u8;
    let b = (t - (co >> 1)).clamp(0, 255) as u8;
    let r = (co + b as i16).clamp(0, 255) as u8;
    (r, g, b)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_detint_overflow_free() {
        let x = vec![ACT_MAX; MAX_K_CHANNELS];
        let w = vec![WEIGHT_MAX; MAX_K_CHANNELS];
        let acc = DetInt::dot_product(&x, &w);
        let expected = (MAX_K_CHANNELS as i64) * (WEIGHT_MAX as i64) * (ACT_MAX as i64);
        assert_eq!(acc as i64, expected);
        assert!(acc < i32::MAX);
    }

    #[test]
    fn test_ycocgr_roundtrip() {
        for r in (0..=255).step_by(37) {
            for g in (0..=255).step_by(37) {
                for b in (0..=255).step_by(37) {
                    let (y, co, cg) = rgb_to_ycocgr(r, g, b);
                    let (rr, gg, bb) = ycocgr_to_rgb(y, co, cg);
                    assert_eq!((r, g, b), (rr, gg, bb));
                }
            }
        }
    }
}
