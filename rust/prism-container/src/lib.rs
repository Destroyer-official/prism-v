//! Prism-V Container Parsers & Multiplexers (Rust Edition)
//!
//! Provides memory-safe implementations of:
//! - `.pvloss` Self-Indexed Lossless Container format
//! - ISO/IEC 14496-12 Dual-Track MP4 structures ('prmr' box)
//! - Rabin Content-Defined Chunking (CDC) deduplication

use std::io::Read;

pub const PVLOSS_MAGIC: [u8; 8] = *b"PRISMV01";
pub const BOX_TYPE_PRSM: u32 = 0x7072736D; // 'prsm'
pub const BOX_TYPE_PRMR: u32 = 0x70726D72; // 'prmr'

#[derive(Debug, Clone)]
pub struct PidxEntry {
    pub virtual_byte_offset: u64,
    pub file_byte_offset: u64,
    pub compressed_size: u32,
    pub uncompressed_size: u32,
    pub crc32c: u32,
}

#[derive(Debug, Clone)]
pub struct PvlossHeader {
    pub magic: [u8; 8],
    pub lossless_mode: u8,
    pub source_codec_id: u8,
    pub flags: u16,
    pub orig_file_size: u64,
    pub orig_sha256: [u8; 32],
    pub width: u16,
    pub height: u16,
    pub fps_num: u16,
    pub fps_den: u16,
    pub bit_depth: u16,
    pub chroma_format: u16,
    pub index_offset: u64,
}

impl Default for PvlossHeader {
    fn default() -> Self {
        Self {
            magic: PVLOSS_MAGIC,
            lossless_mode: 1, // BBR
            source_codec_id: 1, // H.264
            flags: 0x0004, // seekable 256kb
            orig_file_size: 0,
            orig_sha256: [0u8; 32],
            width: 1920,
            height: 1080,
            fps_num: 30,
            fps_den: 1,
            bit_depth: 8,
            chroma_format: 1,
            index_offset: 0,
        }
    }
}

pub struct PvlossContainer;

impl PvlossContainer {
    pub fn read_header<R: Read>(reader: &mut R) -> std::io::Result<PvlossHeader> {
        let mut buf = [0u8; 0x48];
        reader.read_exact(&mut buf)?;

        if &buf[0..8] != PVLOSS_MAGIC {
            return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "Invalid .pvloss magic"));
        }

        let mut orig_sha256 = [0u8; 32];
        orig_sha256.copy_from_slice(&buf[0x14..0x34]);

        Ok(PvlossHeader {
            magic: PVLOSS_MAGIC,
            lossless_mode: buf[0x08],
            source_codec_id: buf[0x09],
            flags: u16::from_le_bytes(buf[0x0A..0x0C].try_into().unwrap()),
            orig_file_size: u64::from_le_bytes(buf[0x0C..0x14].try_into().unwrap()),
            orig_sha256,
            width: u16::from_le_bytes(buf[0x34..0x36].try_into().unwrap()),
            height: u16::from_le_bytes(buf[0x36..0x38].try_into().unwrap()),
            fps_num: u16::from_le_bytes(buf[0x38..0x3A].try_into().unwrap()),
            fps_den: u16::from_le_bytes(buf[0x3A..0x3C].try_into().unwrap()),
            bit_depth: u16::from_le_bytes(buf[0x3C..0x3E].try_into().unwrap()),
            chroma_format: u16::from_le_bytes(buf[0x3E..0x40].try_into().unwrap()),
            index_offset: u64::from_le_bytes(buf[0x40..0x48].try_into().unwrap()),
        })
    }
}
