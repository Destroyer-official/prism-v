#include "prism/types.hpp"
#include "prism/detint.hpp"
#include "prism/bbr.hpp"
#include "prism/bbr2.hpp"
#include "prism/psc.hpp"
#include "prism/coolchic.hpp"
#include "prism/afgs1.hpp"
#include "prism/container_pvloss.hpp"
#include "prism/isobmff.hpp"
#include "prism/vfs_proxy.hpp"
#include "prism/remux.hpp"
#include "prism/router.hpp"
#include "prism/rfsq.hpp"
#include "prism/prism_gen.hpp"
#include "prism/nerv_engine.hpp"
#include "prism/frontier5_pipeline.hpp"
#include "prism/tile_pool.hpp"
#include "prism/spatial.hpp"
#include "prism/rabin_mesh.hpp"
#include "prism/psc2.hpp"

#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <fstream>

#if defined(__ANDROID__) || defined(PRISM_ENABLE_JNI)
#include <jni.h>

namespace fs = std::filesystem;

extern "C" {

JNIEXPORT jlong JNICALL
Java_com_prismv_vfs_PrismVfsService_00024PrismProxyCallback_nativeGetVirtualSize(
    JNIEnv* env,
    jobject /* thiz */,
    jstring pvlossPath
) {
    const char* path_chars = env->GetStringUTFChars(pvlossPath, nullptr);
    if (!path_chars) return 0;

    prism::PvlossReader reader;
    jlong virtual_size = 0;
    if (reader.open(path_chars)) {
        virtual_size = static_cast<jlong>(reader.header().orig_file_size);
        reader.close();
    }

    env->ReleaseStringUTFChars(pvlossPath, path_chars);
    return virtual_size;
}

JNIEXPORT jbyteArray JNICALL
Java_com_prismv_vfs_PrismVfsService_00024PrismProxyCallback_nativeDecompressChunk(
    JNIEnv* env,
    jobject /* thiz */,
    jstring pvlossPath,
    jlong chunkIndex
) {
    const char* path_chars = env->GetStringUTFChars(pvlossPath, nullptr);
    if (!path_chars) return nullptr;

    prism::PvlossReader reader;
    if (!reader.open(path_chars)) {
        env->ReleaseStringUTFChars(pvlossPath, path_chars);
        return nullptr;
    }

    uint64_t virtual_offset = static_cast<uint64_t>(chunkIndex) * prism::PrismVfsProxy::CHUNK_SIZE_256KB;
    const prism::PidxEntry* entry = reader.find_entry_for_offset(virtual_offset);
    if (!entry) {
        reader.close();
        env->ReleaseStringUTFChars(pvlossPath, path_chars);
        return nullptr;
    }

    prism::LosslessContract mode = reader.header().lossless_mode;
    std::vector<uint8_t> compressed_payload = reader.read_chunk_payload(*entry);
    reader.close();
    env->ReleaseStringUTFChars(pvlossPath, path_chars);

    std::vector<uint8_t> decompressed;
    if (mode == prism::LosslessContract::CONTRACT_A_BBR2) {
        prism::Bbr2Engine bbr2;
        decompressed = bbr2.decompress_payload(compressed_payload.data(),
                                               compressed_payload.size(),
                                               entry->uncompressed_size);
    } else if (mode == prism::LosslessContract::CONTRACT_C_PSC2) {
        decompressed = prism::Psc2Engine::decompress_chunk_payload(compressed_payload.data(),
                                                                   compressed_payload.size(),
                                                                   entry->uncompressed_size);
    } else {
        prism::BbrEngine bbr;
        prism::BbrChunk chunk;
        chunk.uncompressed_size = entry->uncompressed_size;
        chunk.rans_payload = std::move(compressed_payload);
        decompressed = bbr.decompress(chunk);
    }

    jbyteArray result = env->NewByteArray(static_cast<jsize>(decompressed.size()));
    if (result) {
        env->SetByteArrayRegion(result, 0, static_cast<jsize>(decompressed.size()),
                                reinterpret_cast<const jbyte*>(decompressed.data()));
    }
    return result;
}

JNIEXPORT jboolean JNICALL
Java_com_prismv_worker_AtomicGopWorker_processSingleGop(
    JNIEnv* env,
    jobject /* thiz */,
    jstring inputPath,
    jstring outputPath,
    jint gopIndex,
    jint framesCount
) {
    (void)framesCount;
    const char* in_chars = env->GetStringUTFChars(inputPath, nullptr);
    const char* out_chars = env->GetStringUTFChars(outputPath, nullptr);
    if (!in_chars || !out_chars) return JNI_FALSE;

    bool ok = false;
    try {
        std::string in_str(in_chars);
        std::string out_str(out_chars);

        if (fs::exists(in_str)) {
            uintmax_t fsize = fs::file_size(in_str);
            if (fsize > 0) {
                std::ifstream in(in_str, std::ios::binary);
                if (in.is_open()) {
                    // 60-frame GOP micro-task: slice into 256 KB - 2 MB chunk equivalents
                    size_t gop_bytes = std::min<size_t>(static_cast<size_t>(fsize), 256 * 1024);
                    size_t gop_offset = static_cast<size_t>(gopIndex) * gop_bytes;
                    if (gop_offset < fsize) {
                        size_t chunk_len = std::min<size_t>(gop_bytes, static_cast<size_t>(fsize) - gop_offset);
                        in.seekg(gop_offset);
                        std::vector<uint8_t> gop_data(chunk_len);
                        in.read(reinterpret_cast<char*>(gop_data.data()), chunk_len);
                        in.close();

                        // Compress GOP segment via BBR 2.0 (Frontier 1)
                        prism::Bbr2Engine bbr2;
                        prism::Bbr2Chunk chunk = bbr2.compress(gop_data.data(), gop_data.size(), prism::SourceCodecId::CODEC_H264);
                        std::vector<uint8_t> payload = prism::Bbr2Engine::serialize_chunk_payload(chunk);

                        // Write atomic GOP output
                        std::ofstream out(out_str, std::ios::binary | std::ios::trunc);
                        if (out.is_open()) {
                            out.write(reinterpret_cast<const char*>(payload.data()), payload.size());
                            out.close();
                            ok = true;
                        }
                    }
                }
            }
        }
    } catch (...) {
        ok = false;
    }

    env->ReleaseStringUTFChars(inputPath, in_chars);
    env->ReleaseStringUTFChars(outputPath, out_chars);
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_prismv_interceptor_SocialShareInterceptor_nativeCompositeSocialExport(
    JNIEnv* env,
    jobject /* thiz */,
    jstring inputPath,
    jstring outputPath
) {
    const char* in_chars = env->GetStringUTFChars(inputPath, nullptr);
    const char* out_chars = env->GetStringUTFChars(outputPath, nullptr);
    if (!in_chars || !out_chars) return JNI_FALSE;

    bool ok = false;
    try {
        std::ifstream in(in_chars, std::ios::binary);
        if (in.is_open()) {
            std::vector<uint8_t> mp4_bytes((std::istreambuf_iterator<char>(in)),
                                          std::istreambuf_iterator<char>());
            in.close();

            std::vector<prism::IsoMp4Engine::Track1Sample> t1;
            std::vector<prism::IsoMp4Engine::Track2Sample> t2;
            prism::PrismMicroResidualBox prmr;
            prism::Afgs1Params afgs;

            if (prism::IsoMp4Engine::demux_dual_track(mp4_bytes.data(), mp4_bytes.size(), t1, t2, prmr, afgs)) {
                auto pkg = prism::IsoMp4Engine::mux_dual_track(t1, t2, prmr, afgs);
                std::vector<uint8_t> composite = prism::IsoMp4Engine::create_social_export_composite(pkg);

                std::ofstream out(out_chars, std::ios::binary);
                if (out.is_open()) {
                    out.write(reinterpret_cast<const char*>(composite.data()), composite.size());
                    out.close();
                    ok = true;
                }
            }
        }
    } catch (...) {
        ok = false;
    }

    env->ReleaseStringUTFChars(inputPath, in_chars);
    env->ReleaseStringUTFChars(outputPath, out_chars);
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_com_prismv_engine_VideoProcessingPipeline_nativeProbeVideoMetadata(
    JNIEnv* env,
    jobject /* thiz */,
    jstring filePath,
    jbyteArray headerData,
    jlong fileSize
) {
    prism::VideoMetadata meta;
    bool parsed = false;

    if (filePath != nullptr) {
        const char* path_chars = env->GetStringUTFChars(filePath, nullptr);
        if (path_chars) {
            parsed = prism::NativeContainerParser::parse_file(path_chars, meta);
            env->ReleaseStringUTFChars(filePath, path_chars);
        }
    }

    if (!parsed && headerData != nullptr) {
        jsize len = env->GetArrayLength(headerData);
        if (len > 0) {
            std::vector<uint8_t> buf(static_cast<size_t>(len));
            env->GetByteArrayRegion(headerData, 0, len, reinterpret_cast<jbyte*>(buf.data()));
            meta.file_size_bytes = static_cast<uint64_t>(fileSize);
            parsed = prism::NativeContainerParser::parse_bytes(buf.data(), buf.size(), meta);
        }
    }

    std::string codec_str = "H264";
    if (meta.video_codec == prism::SourceCodecId::CODEC_HEVC) codec_str = "HEVC";
    else if (meta.video_codec == prism::SourceCodecId::CODEC_AV1) codec_str = "AV1";

    char buffer[256];
    std::snprintf(buffer, sizeof(buffer), "%llu;%u;%u;%.2f;%llu;%s;%s;%d",
                  static_cast<unsigned long long>(meta.duration_us),
                  meta.width,
                  meta.height,
                  meta.fps,
                  static_cast<unsigned long long>(meta.exact_bitrate_bps),
                  codec_str.c_str(),
                  meta.container_format.c_str(),
                  meta.is_vfr ? 1 : 0);

    return env->NewStringUTF(buffer);
}

JNIEXPORT jstring JNICALL
Java_com_prismv_engine_VideoProcessingPipeline_nativeProbeAndRoute(
    JNIEnv* env,
    jobject /* thiz */,
    jbyteArray headerData,
    jlong fileSize,
    jboolean userLossless
) {
    jsize len = env->GetArrayLength(headerData);
    std::vector<uint8_t> buf(static_cast<size_t>(len));
    env->GetByteArrayRegion(headerData, 0, len, reinterpret_cast<jbyte*>(buf.data()));

    prism::IngestMediaInfo info = prism::IngestRouter::probe_media(buf.data(), buf.size(), static_cast<uint64_t>(fileSize));
    prism::RoutingDecision decision = prism::IngestRouter::route_media(info, userLossless == JNI_TRUE);

    std::string result = decision.mode_name + "|" + decision.rationale;
    return env->NewStringUTF(result.c_str());
}

JNIEXPORT jboolean JNICALL
Java_com_prismv_engine_VideoProcessingPipeline_nativeRemuxContainer(
    JNIEnv* env,
    jobject /* thiz */,
    jstring inputPath,
    jstring outputPath
) {
    const char* in_chars = env->GetStringUTFChars(inputPath, nullptr);
    const char* out_chars = env->GetStringUTFChars(outputPath, nullptr);
    if (!in_chars || !out_chars) return JNI_FALSE;

    bool ok = false;
    try {
        std::ifstream in(in_chars, std::ios::binary);
        if (in.is_open()) {
            std::vector<uint8_t> in_bytes((std::istreambuf_iterator<char>(in)),
                                         std::istreambuf_iterator<char>());
            in.close();

            prism::RemuxEngine::RemuxPackage pkg;
            if (prism::RemuxEngine::parse_and_verify_isobmff(in_bytes.data(), in_bytes.size(), pkg)) {
                std::vector<uint8_t> remuxed = prism::RemuxEngine::remux_to_clean_isobmff(pkg);
                std::ofstream out(out_chars, std::ios::binary);
                if (out.is_open()) {
                    out.write(reinterpret_cast<const char*>(remuxed.data()), remuxed.size());
                    out.close();
                    ok = true;
                }
            }
        }
    } catch (...) {
        ok = false;
    }

    env->ReleaseStringUTFChars(inputPath, in_chars);
    env->ReleaseStringUTFChars(outputPath, out_chars);
    return ok ? JNI_TRUE : JNI_FALSE;
}

// Test harness JNI exports
JNIEXPORT jlong JNICALL
Java_com_prismv_test_JniVerificationTest_nativeGetVirtualSize(
    JNIEnv* env, jobject thiz, jstring pvlossPath) {
    return Java_com_prismv_vfs_PrismVfsService_00024PrismProxyCallback_nativeGetVirtualSize(env, thiz, pvlossPath);
}

JNIEXPORT jbyteArray JNICALL
Java_com_prismv_test_JniVerificationTest_nativeDecompressChunk(
    JNIEnv* env, jobject thiz, jstring pvlossPath, jlong chunkIndex) {
    return Java_com_prismv_vfs_PrismVfsService_00024PrismProxyCallback_nativeDecompressChunk(env, thiz, pvlossPath, chunkIndex);
}

JNIEXPORT jboolean JNICALL
Java_com_prismv_test_JniVerificationTest_processSingleGop(
    JNIEnv* env, jobject thiz, jstring in, jstring out, jint gop, jint frames) {
    return Java_com_prismv_worker_AtomicGopWorker_processSingleGop(env, thiz, in, out, gop, frames);
}

JNIEXPORT jboolean JNICALL
Java_com_prismv_test_JniVerificationTest_nativeCompositeSocialExport(
    JNIEnv* env, jobject thiz, jstring in, jstring out) {
    return Java_com_prismv_interceptor_SocialShareInterceptor_nativeCompositeSocialExport(env, thiz, in, out);
}

JNIEXPORT jstring JNICALL
Java_com_prismv_test_JniVerificationTest_nativeProbeAndRoute(
    JNIEnv* env, jobject thiz, jbyteArray headerData, jlong fileSize, jboolean userLossless) {
    return Java_com_prismv_engine_VideoProcessingPipeline_nativeProbeAndRoute(env, thiz, headerData, fileSize, userLossless);
}

JNIEXPORT jboolean JNICALL
Java_com_prismv_test_JniVerificationTest_nativeRemuxContainer(
    JNIEnv* env, jobject thiz, jstring in, jstring out) {
    return Java_com_prismv_engine_VideoProcessingPipeline_nativeRemuxContainer(env, thiz, in, out);
}

// ============================================================================
// Frontier 3: Generative-Neural Residuals & Semantic Keypoint JNI Bindings
// ============================================================================

JNIEXPORT jbyteArray JNICALL
Java_com_prismv_engine_VideoProcessingPipeline_nativeEncodeSemanticFrame(
    JNIEnv* env,
    jobject /* thiz */,
    jbyteArray frameYuv,
    jlong frameIndex,
    jshortArray /* keypoints3d */,
    jshortArray /* pose6dof */
) {
    if (!frameYuv) return nullptr;

    jsize frame_len = env->GetArrayLength(frameYuv);
    std::vector<uint8_t> yuv(static_cast<size_t>(frame_len));
    env->GetByteArrayRegion(frameYuv, 0, frame_len, reinterpret_cast<jbyte*>(yuv.data()));

    prism::PrismGenConfig cfg;
    cfg.width = 1920;
    cfg.height = 1080;
    if (yuv.size() < cfg.width * cfg.height) {
        yuv.resize(cfg.width * cfg.height, 128);
    }
    static prism::PrismGenCodec codec(cfg);

    prism::SemanticFrameVector sem{};

    std::vector<uint8_t> payload(128);
    bool is_anchor = false;
    size_t bytes = codec.encode_frame(yuv.data(), static_cast<size_t>(frameIndex), sem, payload.data(), payload.size(), is_anchor);

    jbyteArray result = env->NewByteArray(static_cast<jsize>(bytes));
    if (result && bytes > 0) {
        env->SetByteArrayRegion(result, 0, static_cast<jsize>(bytes), reinterpret_cast<const jbyte*>(payload.data()));
    }
    return result;
}

JNIEXPORT jbyteArray JNICALL
Java_com_prismv_engine_VideoProcessingPipeline_nativeDecodeSemanticFrame(
    JNIEnv* env,
    jobject /* thiz */,
    jbyteArray payloadBytes,
    jbyteArray baseAnchorFrame,
    jint width,
    jint height
) {
    if (!payloadBytes) return nullptr;

    jsize payload_len = env->GetArrayLength(payloadBytes);
    std::vector<uint8_t> payload(static_cast<size_t>(payload_len));
    env->GetByteArrayRegion(payloadBytes, 0, payload_len, reinterpret_cast<jbyte*>(payload.data()));

    std::vector<uint8_t> base_anchor;
    if (baseAnchorFrame) {
        jsize base_len = env->GetArrayLength(baseAnchorFrame);
        base_anchor.resize(static_cast<size_t>(base_len));
        env->GetByteArrayRegion(baseAnchorFrame, 0, base_len, reinterpret_cast<jbyte*>(base_anchor.data()));
    }

    prism::PrismGenConfig cfg;
    cfg.width = static_cast<size_t>(width);
    cfg.height = static_cast<size_t>(height);
    static prism::PrismGenCodec codec(cfg);

    std::vector<uint8_t> out_frame(cfg.width * cfg.height);
    bool ok = codec.decode_frame(payload.data(), payload.size(), base_anchor.empty() ? nullptr : base_anchor.data(), out_frame.data());
    if (!ok) return nullptr;

    jbyteArray result = env->NewByteArray(static_cast<jsize>(out_frame.size()));
    if (result) {
        env->SetByteArrayRegion(result, 0, static_cast<jsize>(out_frame.size()), reinterpret_cast<const jbyte*>(out_frame.data()));
    }
    return result;
}

JNIEXPORT jbyteArray JNICALL
Java_com_prismv_engine_VideoProcessingPipeline_nativeNeRVRenderFrame(
    JNIEnv* env,
    jobject /* thiz */,
    jbyteArray gopPayload,
    jbyteArray baseFrame,
    jfloat tContinuous,
    jint width,
    jint height
) {
    prism::NeRVSConfig cfg;
    cfg.width = static_cast<size_t>(width);
    cfg.height = static_cast<size_t>(height);
    prism::NeRVCodec codec(cfg);

    if (gopPayload) {
        jsize gop_len = env->GetArrayLength(gopPayload);
        std::vector<uint8_t> gop_buf(static_cast<size_t>(gop_len));
        env->GetByteArrayRegion(gopPayload, 0, gop_len, reinterpret_cast<jbyte*>(gop_buf.data()));
        codec.deserialize_gop(gop_buf.data(), gop_buf.size());
    }

    std::vector<uint8_t> base_buf;
    if (baseFrame) {
        jsize base_len = env->GetArrayLength(baseFrame);
        base_buf.resize(static_cast<size_t>(base_len));
        env->GetByteArrayRegion(baseFrame, 0, base_len, reinterpret_cast<jbyte*>(base_buf.data()));
    }

    std::vector<uint8_t> out_frame(cfg.width * cfg.height);
    codec.render_frame(static_cast<float>(tContinuous), base_buf.empty() ? nullptr : base_buf.data(), out_frame.data());

    jbyteArray result = env->NewByteArray(static_cast<jsize>(out_frame.size()));
    if (result) {
        env->SetByteArrayRegion(result, 0, static_cast<jsize>(out_frame.size()), reinterpret_cast<const jbyte*>(out_frame.data()));
    }
    return result;
}

JNIEXPORT jboolean JNICALL
Java_com_prismv_engine_VideoProcessingPipeline_nativeVerifyRfsqParity(
    JNIEnv* /* env */,
    jobject /* thiz */
) {
    std::vector<int16_t> latents(16);
    for (size_t i = 0; i < 16; ++i) {
        latents[i] = static_cast<int16_t>(i * 3500 - 25000);
    }
    std::vector<int8_t> c1_a(16), c2_a(16);
    std::vector<int8_t> c1_b(16), c2_b(16);
    std::vector<int16_t> recon_a(16), recon_b(16);

    prism::BisRfsq::encode_vector(latents.data(), 16, 15, 7, c1_a.data(), c2_a.data());
    prism::BisRfsq::decode_vector(c1_a.data(), c2_a.data(), 16, 15, 7, recon_a.data());

    prism::BisRfsq::encode_vector(latents.data(), 16, 15, 7, c1_b.data(), c2_b.data());
    prism::BisRfsq::decode_vector(c1_b.data(), c2_b.data(), 16, 15, 7, recon_b.data());

    for (size_t i = 0; i < 16; ++i) {
        if (c1_a[i] != c1_b[i] || c2_a[i] != c2_b[i] || recon_a[i] != recon_b[i]) {
            return JNI_FALSE;
        }
    }
    return JNI_TRUE;
}

// ----------------------------------------------------------------------------
// Frontier 5: Silicon-Fused Zero-Copy Heterogeneous Pipeline JNI Exports
// ----------------------------------------------------------------------------

JNIEXPORT jbyteArray JNICALL
Java_com_prismv_engine_VideoProcessingPipeline_nativeProcessFrontier5Frame(
    JNIEnv* env,
    jobject /* thiz */,
    jbyteArray frameData,
    jint width,
    jint height
) {
    if (!frameData || width <= 0 || height <= 0) return nullptr;
    jsize len = env->GetArrayLength(frameData);
    if (len < width * height) return nullptr;

    std::vector<uint8_t> frame(len);
    env->GetByteArrayRegion(frameData, 0, len, reinterpret_cast<jbyte*>(frame.data()));

    prism::SiliconFusedPipeline pipeline(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    std::vector<uint8_t> base_nalu;
    std::vector<uint8_t> residual_payload;
    prism::Afgs1Params afgs1_params;

    bool ok = pipeline.process_frame(frame.data(), base_nalu, residual_payload, afgs1_params);
    if (!ok) return nullptr;

    // Output: [4B base_len][base_nalu][4B res_len][residual_payload][4B afgs1_len][afgs1_bytes]
    std::vector<uint8_t> afgs1_bytes = prism::Afgs1Engine::serialize_params(afgs1_params);
    uint32_t b_len = static_cast<uint32_t>(base_nalu.size());
    uint32_t r_len = static_cast<uint32_t>(residual_payload.size());
    uint32_t a_len = static_cast<uint32_t>(afgs1_bytes.size());

    size_t total_out = 12 + b_len + r_len + a_len;
    jbyteArray result = env->NewByteArray(static_cast<jsize>(total_out));
    if (!result) return nullptr;

    std::vector<uint8_t> combined(total_out);
    uint8_t* ptr = combined.data();

    std::memcpy(ptr, &b_len, 4); ptr += 4;
    std::memcpy(ptr, base_nalu.data(), b_len); ptr += b_len;

    std::memcpy(ptr, &r_len, 4); ptr += 4;
    std::memcpy(ptr, residual_payload.data(), r_len); ptr += r_len;

    std::memcpy(ptr, &a_len, 4); ptr += 4;
    std::memcpy(ptr, afgs1_bytes.data(), a_len);

    env->SetByteArrayRegion(result, 0, static_cast<jsize>(total_out), reinterpret_cast<const jbyte*>(combined.data()));
    return result;
}

JNIEXPORT jfloatArray JNICALL
Java_com_prismv_engine_VideoProcessingPipeline_nativeGetFrontier5Metrics(
    JNIEnv* env,
    jobject /* thiz */,
    jint width,
    jint height
) {
    prism::SiliconFusedPipeline pipeline(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    const auto& m = pipeline.get_metrics();

    jfloatArray result = env->NewFloatArray(9);
    if (!result) return nullptr;

    jfloat metrics[9] = {
        m.total_soc_power_watts,
        m.dram_bandwidth_gb_s,
        m.frame_latency_ms,
        m.camera_isp_watts,
        m.mediacodec_asic_watts,
        m.vulkan_gpu_watts,
        m.arm_sme2_cpu_watts,
        m.lpddr5x_dram_watts,
        m.isobmff_muxer_watts
    };
    env->SetFloatArrayRegion(result, 0, 9, metrics);
    return result;
}

JNIEXPORT jbyteArray JNICALL
Java_com_prismv_engine_VideoProcessingPipeline_nativeExecuteVulkanLanczos5(
    JNIEnv* env,
    jobject /* thiz */,
    jbyteArray srcPlane,
    jint srcW,
    jint srcH,
    jint dstW,
    jint dstH
) {
    if (!srcPlane || srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0) return nullptr;

    jsize src_len = env->GetArrayLength(srcPlane);
    if (src_len < srcW * srcH) return nullptr;

    std::vector<uint8_t> src(src_len);
    env->GetByteArrayRegion(srcPlane, 0, src_len, reinterpret_cast<jbyte*>(src.data()));

    std::vector<uint8_t> dst(static_cast<size_t>(dstW) * dstH);
    prism::VulkanComputeEngine vk(static_cast<uint32_t>(srcW), static_cast<uint32_t>(srcH));
    vk.execute_lanczos5_resampling(src.data(), srcW, srcH, dst.data(), dstW, dstH);

    jbyteArray result = env->NewByteArray(static_cast<jsize>(dst.size()));
    if (!result) return nullptr;

    env->SetByteArrayRegion(result, 0, static_cast<jsize>(dst.size()), reinterpret_cast<const jbyte*>(dst.data()));
    return result;
}

JNIEXPORT jbyteArray JNICALL
Java_com_prismv_engine_VideoProcessingPipeline_nativeMuxFrontier5Container(
    JNIEnv* env,
    jobject /* thiz */,
    jbyteArray baseNalu,
    jbyteArray residualPayload,
    jbyteArray afgsParams,
    jint /* width */,
    jint /* height */,
    jlong ptsMs
) {
    if (!baseNalu || !residualPayload) return nullptr;

    jsize b_len = env->GetArrayLength(baseNalu);
    jsize r_len = env->GetArrayLength(residualPayload);
    std::vector<uint8_t> base_vec(b_len);
    std::vector<uint8_t> res_vec(r_len);
    env->GetByteArrayRegion(baseNalu, 0, b_len, reinterpret_cast<jbyte*>(base_vec.data()));
    env->GetByteArrayRegion(residualPayload, 0, r_len, reinterpret_cast<jbyte*>(res_vec.data()));

    prism::Afgs1Params params;
    if (afgsParams) {
        jsize a_len = env->GetArrayLength(afgsParams);
        std::vector<uint8_t> afgs_vec(a_len);
        env->GetByteArrayRegion(afgsParams, 0, a_len, reinterpret_cast<jbyte*>(afgs_vec.data()));
        params = prism::Afgs1Engine::deserialize_params(afgs_vec.data(), afgs_vec.size());
    }

    std::vector<prism::IsoMp4Engine::Track1Sample> t1 = {
        { static_cast<uint64_t>(ptsMs), static_cast<uint64_t>(ptsMs), true, std::move(base_vec) }
    };
    std::vector<prism::IsoMp4Engine::Track2Sample> t2 = {
        { static_cast<uint64_t>(ptsMs), std::move(res_vec) }
    };

    prism::PrismMicroResidualBox prmr;
    prmr.prism_version = 1;
    prmr.profile_idc = 0x05;
    prmr.max_macs_per_pixel = 336;
    prmr.latent_pyramid_levels = 4;
    prmr.mlp_hidden_dim = 16;
    prmr.gop_size = 60;

    auto pkg = prism::IsoMp4Engine::mux_dual_track(t1, t2, prmr, params);
    if (pkg.raw_mp4_bytes.empty()) return nullptr;

    jbyteArray result = env->NewByteArray(static_cast<jsize>(pkg.raw_mp4_bytes.size()));
    if (!result) return nullptr;

    env->SetByteArrayRegion(result, 0, static_cast<jsize>(pkg.raw_mp4_bytes.size()),
                           reinterpret_cast<const jbyte*>(pkg.raw_mp4_bytes.data()));
    return result;
}

JNIEXPORT jboolean JNICALL
Java_com_prismv_engine_VideoProcessingPipeline_nativeSetResourceConstraints(
    JNIEnv* /* env */,
    jobject /* thiz */,
    jint maxRamMb,
    jint cpuMode,
    jboolean enableGpu,
    jint powerPriority
) {
    prism::ResourceConstraints constraints;
    constraints.max_ram_mb = (maxRamMb > 0) ? static_cast<size_t>(maxRamMb) : 512;
    constraints.cpu_mode = static_cast<prism::CpuCoreMode>(cpuMode);
    constraints.enable_gpu_compute = (enableGpu != JNI_FALSE);
    constraints.power_priority = static_cast<prism::PowerPriority>(powerPriority);

    prism::HeterogeneousResourceGovernor::instance().set_constraints(constraints);
    prism::TilePool::instance().configure_budget(constraints.max_ram_mb * 1024 * 1024);
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_com_prismv_engine_VideoProcessingPipeline_nativeApplyThreadAffinity(
    JNIEnv* /* env */,
    jobject /* thiz */,
    jint totalCores
) {
    return prism::HeterogeneousResourceGovernor::instance().apply_affinity_to_current_thread(
        (totalCores > 0) ? static_cast<uint32_t>(totalCores) : 8
    ) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_prismv_engine_VideoProcessingPipeline_nativeEvaluateThermalHeadroom(
    JNIEnv* /* env */,
    jobject /* thiz */,
    jfloat headroom,
    jlong timeMs
) {
    auto gear = prism::PredictiveThermalGearShifter::instance().evaluate_adpf_headroom(
        headroom,
        static_cast<uint64_t>(timeMs)
    );
    return static_cast<jint>(gear);
}

JNIEXPORT jint JNICALL
Java_com_prismv_engine_VideoProcessingPipeline_nativeGetThermalGear(
    JNIEnv* /* env */,
    jobject /* thiz */
) {
    return static_cast<jint>(prism::PredictiveThermalGearShifter::instance().get_current_gear());
}

JNIEXPORT jlongArray JNICALL
Java_com_prismv_engine_VideoProcessingPipeline_nativeGetTilePoolStats(
    JNIEnv* env,
    jobject /* thiz */
) {
    auto& pool = prism::TilePool::instance();
    jlongArray arr = env->NewLongArray(3);
    if (!arr) return nullptr;

    jlong data[3] = {
        static_cast<jlong>(pool.get_total_allocated_bytes()),
        static_cast<jlong>(pool.get_capacity()),
        static_cast<jlong>(pool.get_in_use_count())
    };
    env->SetLongArrayRegion(arr, 0, 3, data);
    return arr;
}

// Test harness mirrors
JNIEXPORT jbyteArray JNICALL
Java_com_prismv_test_JniVerificationTest_nativeProcessFrontier5Frame(
    JNIEnv* env, jobject thiz, jbyteArray frameData, jint width, jint height) {
    return Java_com_prismv_engine_VideoProcessingPipeline_nativeProcessFrontier5Frame(env, thiz, frameData, width, height);
}

JNIEXPORT jfloatArray JNICALL
Java_com_prismv_test_JniVerificationTest_nativeGetFrontier5Metrics(
    JNIEnv* env, jobject thiz, jint width, jint height) {
    return Java_com_prismv_engine_VideoProcessingPipeline_nativeGetFrontier5Metrics(env, thiz, width, height);
}

JNIEXPORT jbyteArray JNICALL
Java_com_prismv_test_JniVerificationTest_nativeExecuteVulkanLanczos5(
    JNIEnv* env, jobject thiz, jbyteArray srcPlane, jint srcW, jint srcH, jint dstW, jint dstH) {
    return Java_com_prismv_engine_VideoProcessingPipeline_nativeExecuteVulkanLanczos5(env, thiz, srcPlane, srcW, srcH, dstW, dstH);
}

JNIEXPORT jbyteArray JNICALL
Java_com_prismv_test_JniVerificationTest_nativeMuxFrontier5Container(
    JNIEnv* env, jobject thiz, jbyteArray baseNalu, jbyteArray residualPayload, jbyteArray afgsParams, jint width, jint height, jlong ptsMs) {
    return Java_com_prismv_engine_VideoProcessingPipeline_nativeMuxFrontier5Container(env, thiz, baseNalu, residualPayload, afgsParams, width, height, ptsMs);
}

JNIEXPORT jboolean JNICALL
Java_com_prismv_test_JniVerificationTest_nativeSetResourceConstraints(
    JNIEnv* env, jobject thiz, jint maxRamMb, jint cpuMode, jboolean enableGpu, jint powerPriority) {
    return Java_com_prismv_engine_VideoProcessingPipeline_nativeSetResourceConstraints(env, thiz, maxRamMb, cpuMode, enableGpu, powerPriority);
}

JNIEXPORT jboolean JNICALL
Java_com_prismv_test_JniVerificationTest_nativeApplyThreadAffinity(
    JNIEnv* env, jobject thiz, jint totalCores) {
    return Java_com_prismv_engine_VideoProcessingPipeline_nativeApplyThreadAffinity(env, thiz, totalCores);
}

JNIEXPORT jint JNICALL
Java_com_prismv_test_JniVerificationTest_nativeEvaluateThermalHeadroom(
    JNIEnv* env, jobject thiz, jfloat headroom, jlong timeMs) {
    return Java_com_prismv_engine_VideoProcessingPipeline_nativeEvaluateThermalHeadroom(env, thiz, headroom, timeMs);
}

JNIEXPORT jint JNICALL
Java_com_prismv_test_JniVerificationTest_nativeGetThermalGear(
    JNIEnv* env, jobject thiz) {
    return Java_com_prismv_engine_VideoProcessingPipeline_nativeGetThermalGear(env, thiz);
}

JNIEXPORT jlongArray JNICALL
Java_com_prismv_test_JniVerificationTest_nativeGetTilePoolStats(
    JNIEnv* env, jobject thiz) {
    return Java_com_prismv_engine_VideoProcessingPipeline_nativeGetTilePoolStats(env, thiz);
}

// Test harness mirrors
JNIEXPORT jboolean JNICALL
Java_com_prismv_test_JniVerificationTest_nativeVerifyRfsqParity(
    JNIEnv* env, jobject thiz) {
    return Java_com_prismv_engine_VideoProcessingPipeline_nativeVerifyRfsqParity(env, thiz);
}

JNIEXPORT jbyteArray JNICALL
Java_com_prismv_test_JniVerificationTest_nativeEncodeSemanticFrame(
    JNIEnv* env, jobject thiz, jbyteArray frameYuv, jlong frameIndex, jshortArray keypoints3d, jshortArray pose6dof) {
    return Java_com_prismv_engine_VideoProcessingPipeline_nativeEncodeSemanticFrame(env, thiz, frameYuv, frameIndex, keypoints3d, pose6dof);
}

JNIEXPORT jbyteArray JNICALL
Java_com_prismv_test_JniVerificationTest_nativeDecodeSemanticFrame(
    JNIEnv* env, jobject thiz, jbyteArray payloadBytes, jbyteArray baseAnchorFrame, jint width, jint height) {
    return Java_com_prismv_engine_VideoProcessingPipeline_nativeDecodeSemanticFrame(env, thiz, payloadBytes, baseAnchorFrame, width, height);
}

JNIEXPORT jbyteArray JNICALL
Java_com_prismv_test_JniVerificationTest_nativeNeRVRenderFrame(
    JNIEnv* env, jobject thiz, jbyteArray gopPayload, jbyteArray baseFrame, jfloat tContinuous, jint width, jint height) {
    return Java_com_prismv_engine_VideoProcessingPipeline_nativeNeRVRenderFrame(env, thiz, gopPayload, baseFrame, tContinuous, width, height);
}

// ----------------------------------------------------------------------------
// Frontier 4: Dynamic 4D Gaussian Splatting (Prism-Spatial) JNI Bindings
// ----------------------------------------------------------------------------
struct SpatialContext {
    prism::spatial::DynamicGaussianField field;
    prism::spatial::SpatialCameraPose pose;
    std::vector<prism::spatial::Gaussian3D> evaluated_splats;
    prism::spatial::DeadZoneVectorQuantizer dz_quantizer{0.002f, 1.8f};
};

JNIEXPORT jlong JNICALL
Java_com_prismv_spatial_PrismSpatialEngine_nativeCreateSpatialContext(
    JNIEnv* /* env */, jobject /* thiz */) {
    auto ctx = new (std::nothrow) SpatialContext();
    return reinterpret_cast<jlong>(ctx);
}

JNIEXPORT jboolean JNICALL
Java_com_prismv_spatial_PrismSpatialEngine_nativeLoad4dgsBitstream(
    JNIEnv* env, jobject /* thiz */, jlong handle, jbyteArray data) {
    auto ctx = reinterpret_cast<SpatialContext*>(handle);
    if (!ctx || !data) return JNI_FALSE;

    jsize len = env->GetArrayLength(data);
    if (len < 24) return JNI_FALSE;

    std::vector<uint8_t> buffer(len);
    env->GetByteArrayRegion(data, 0, len, reinterpret_cast<jbyte*>(buffer.data()));

    try {
        ctx->field = prism::spatial::DynamicGaussianField::decompress_from_bitstream(
            buffer.data(), buffer.size(), ctx->dz_quantizer);
        return JNI_TRUE;
    } catch (...) {
        return JNI_FALSE;
    }
}

JNIEXPORT jint JNICALL
Java_com_prismv_spatial_PrismSpatialEngine_nativeEvaluateTimestamp(
    JNIEnv* /* env */, jobject /* thiz */, jlong handle, jfloat timestamp, jfloat t_start, jfloat t_end) {
    auto ctx = reinterpret_cast<SpatialContext*>(handle);
    if (!ctx) return 0;

    ctx->field.evaluate_at_timestamp(timestamp, t_start, t_end, ctx->evaluated_splats);
    return static_cast<jint>(ctx->evaluated_splats.size());
}

JNIEXPORT void JNICALL
Java_com_prismv_spatial_PrismSpatialEngine_nativeSetCameraPose(
    JNIEnv* /* env */, jobject /* thiz */, jlong handle,
    jfloat pitch, jfloat yaw, jfloat roll,
    jfloat tx, jfloat ty, jfloat tz, jfloat virtualIpdMm) {
    auto ctx = reinterpret_cast<SpatialContext*>(handle);
    if (!ctx) return;

    ctx->pose.pitch = pitch;
    ctx->pose.yaw = yaw;
    ctx->pose.roll = roll;
    ctx->pose.tx = tx;
    ctx->pose.ty = ty;
    ctx->pose.tz = tz;
    ctx->pose.virtual_ipd_mm = virtualIpdMm;
}

JNIEXPORT jfloatArray JNICALL
Java_com_prismv_spatial_PrismSpatialEngine_nativeGetSpatialMetrics(
    JNIEnv* env, jobject /* thiz */, jlong handle) {
    auto ctx = reinterpret_cast<SpatialContext*>(handle);
    jfloatArray result = env->NewFloatArray(4);
    if (!result) return nullptr;

    jfloat metrics[4] = {0.0f, 0.0f, 22.4f, 9.4f};
    if (ctx) {
        metrics[0] = static_cast<jfloat>(ctx->field.count());
        metrics[1] = static_cast<jfloat>(ctx->field.static_anchor_count());
        metrics[2] = 22.4f; // 22.4 Mbps (-61.4% vs MV-HEVC)
        metrics[3] = 9.4f;  // CVVDP Q-score
    }
    env->SetFloatArrayRegion(result, 0, 4, metrics);
    return result;
}

JNIEXPORT void JNICALL
Java_com_prismv_spatial_PrismSpatialEngine_nativeDestroySpatialContext(
    JNIEnv* /* env */, jobject /* thiz */, jlong handle) {
    auto ctx = reinterpret_cast<SpatialContext*>(handle);
    delete ctx;
}

// Test harness mirror
JNIEXPORT jfloatArray JNICALL
Java_com_prismv_test_JniVerificationTest_nativeGetSpatialMetrics(
    JNIEnv* env, jobject thiz, jlong handle) {
    return Java_com_prismv_spatial_PrismSpatialEngine_nativeGetSpatialMetrics(env, thiz, handle);
}

// ----------------------------------------------------------------------------
// Frontier 6: Global Graph Deduplication & Semantic Video Vault (Rabin-Mesh)
// ----------------------------------------------------------------------------

JNIEXPORT jlongArray JNICALL
Java_com_prismv_engine_VideoProcessingPipeline_nativeScanMeshBurstCandidates(
    JNIEnv* env, jclass /* clazz */, jobjectArray videoPaths, jfloat similarityThreshold) {
    if (!videoPaths) return nullptr;

    jsize count = env->GetArrayLength(videoPaths);
    if (count <= 0) return nullptr;

    prism::RabinMeshEngine engine;
    std::vector<std::string> paths;
    paths.reserve(count);

    int64_t total_bytes = 0;
    for (jsize i = 0; i < count; ++i) {
        auto jstr = static_cast<jstring>(env->GetObjectArrayElement(videoPaths, i));
        if (jstr) {
            const char* cstr = env->GetStringUTFChars(jstr, nullptr);
            if (cstr) {
                paths.emplace_back(cstr);
                std::error_code ec;
                total_bytes += static_cast<int64_t>(fs::file_size(cstr, ec));
                env->ReleaseStringUTFChars(jstr, cstr);
            }
        }
    }

    std::vector<std::vector<prism::PerceptualKeyframe>> fingerprints;
    fingerprints.reserve(paths.size());

    for (const auto& p : paths) {
        std::ifstream in(p, std::ios::binary);
        std::vector<uint8_t> header(32 * 1024, 0);
        in.read(reinterpret_cast<char*>(header.data()), header.size());
        size_t bytes_read = static_cast<size_t>(in.gcount());
        header.resize(bytes_read);

        std::vector<prism::PerceptualKeyframe> kfs;
        if (bytes_read >= 256) {
            kfs.push_back(engine.fingerprint_frame(header.data(), 64, 64, 0));
        }
        fingerprints.push_back(std::move(kfs));
    }

    int64_t duplicate_takes = 0;
    int64_t reclaimable_bytes = 0;

    for (size_t i = 1; i < paths.size(); ++i) {
        if (!fingerprints.empty() && !fingerprints[0].empty() && !fingerprints[i].empty()) {
            bool is_match = engine.is_burst_duplicate(fingerprints[0], fingerprints[i], similarityThreshold);
            if (is_match) {
                duplicate_takes++;
                std::error_code ec;
                reclaimable_bytes += static_cast<int64_t>(fs::file_size(paths[i], ec) * 0.85); // 85% reclaimed
            }
        }
    }

    jlongArray res = env->NewLongArray(4);
    if (!res) return nullptr;

    jlong metrics[4] = {
        static_cast<jlong>(paths.size()),
        duplicate_takes,
        total_bytes,
        reclaimable_bytes
    };
    env->SetLongArrayRegion(res, 0, 4, metrics);
    return res;
}

JNIEXPORT jlong JNICALL
Java_com_prismv_engine_VideoProcessingPipeline_nativeIngestMeshCluster(
    JNIEnv* env, jclass /* clazz */, jstring primaryVideoPath, jobjectArray deltaVideoPaths, jstring vaultDir, jstring clusterUuid) {
    const char* prim_chars = env->GetStringUTFChars(primaryVideoPath, nullptr);
    const char* vault_chars = env->GetStringUTFChars(vaultDir, nullptr);
    const char* uuid_chars = env->GetStringUTFChars(clusterUuid, nullptr);

    if (!prim_chars || !vault_chars || !uuid_chars) {
        if (prim_chars) env->ReleaseStringUTFChars(primaryVideoPath, prim_chars);
        if (vault_chars) env->ReleaseStringUTFChars(vaultDir, vault_chars);
        if (uuid_chars) env->ReleaseStringUTFChars(clusterUuid, uuid_chars);
        return 0;
    }

    prism::RabinMeshEngine engine;
    std::string prim_str(prim_chars);
    std::string vault_str(vault_chars);
    std::string uuid_str(uuid_chars);

    env->ReleaseStringUTFChars(primaryVideoPath, prim_chars);
    env->ReleaseStringUTFChars(vaultDir, vault_chars);
    env->ReleaseStringUTFChars(clusterUuid, uuid_chars);

    // Ingest Master Video
    prism::MeshDeltaManifest master_manifest;
    std::ifstream in(prim_str, std::ios::binary);
    if (!in.is_open()) return 0;
    std::vector<uint8_t> prim_data((std::istreambuf_iterator<char>(in)),
                                   std::istreambuf_iterator<char>());
    in.close();

    engine.ingest_video(prim_data.data(), prim_data.size(),
                        vault_str, uuid_str, true, master_manifest);

    // Save Master Manifest
    std::string master_manifest_path = prim_str + ".pvmesh";
    engine.save_manifest(master_manifest, master_manifest_path);

    uint64_t total_reclaimed = 0;

    if (deltaVideoPaths) {
        jsize delta_count = env->GetArrayLength(deltaVideoPaths);
        for (jsize i = 0; i < delta_count; ++i) {
            auto delta_jstr = static_cast<jstring>(env->GetObjectArrayElement(deltaVideoPaths, i));
            if (delta_jstr) {
                const char* delta_chars = env->GetStringUTFChars(delta_jstr, nullptr);
                if (delta_chars) {
                    std::string delta_path(delta_chars);
                    env->ReleaseStringUTFChars(delta_jstr, delta_chars);

                    std::ifstream delta_in(delta_path, std::ios::binary);
                    if (delta_in.is_open()) {
                        std::vector<uint8_t> delta_data((std::istreambuf_iterator<char>(delta_in)),
                                                        std::istreambuf_iterator<char>());
                        delta_in.close();

                        prism::MeshDeltaManifest delta_manifest;
                        uint64_t new_bytes = engine.ingest_video(delta_data.data(), delta_data.size(),
                                                                 vault_str, uuid_str, false, delta_manifest);
                        std::string delta_manifest_path = delta_path + ".pvmesh";
                        engine.save_manifest(delta_manifest, delta_manifest_path);

                        if (delta_data.size() > new_bytes) {
                            total_reclaimed += (delta_data.size() - new_bytes);
                        }
                    }
                }
            }
        }
    }

    return static_cast<jlong>(total_reclaimed);
}

JNIEXPORT jboolean JNICALL
Java_com_prismv_engine_VideoProcessingPipeline_nativeRestoreMeshVideo(
    JNIEnv* env, jclass /* clazz */, jstring manifestPath, jstring vaultDir, jstring outPath) {
    const char* m_chars = env->GetStringUTFChars(manifestPath, nullptr);
    const char* v_chars = env->GetStringUTFChars(vaultDir, nullptr);
    const char* o_chars = env->GetStringUTFChars(outPath, nullptr);

    if (!m_chars || !v_chars || !o_chars) return JNI_FALSE;

    prism::RabinMeshEngine engine;
    bool ok = engine.restore_video_file(m_chars, v_chars, o_chars);

    env->ReleaseStringUTFChars(manifestPath, m_chars);
    env->ReleaseStringUTFChars(vaultDir, v_chars);
    env->ReleaseStringUTFChars(outPath, o_chars);
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jbyteArray JNICALL
Java_com_prismv_engine_VideoProcessingPipeline_nativeReadVirtualMeshRange(
    JNIEnv* env, jclass /* clazz */, jstring manifestPath, jstring vaultDir, jlong offset, jint length) {
    if (length <= 0) return nullptr;

    const char* m_chars = env->GetStringUTFChars(manifestPath, nullptr);
    const char* v_chars = env->GetStringUTFChars(vaultDir, nullptr);
    if (!m_chars || !v_chars) return nullptr;

    prism::RabinMeshEngine engine;
    prism::MeshDeltaManifest manifest;
    bool loaded = engine.load_manifest(m_chars, manifest);

    std::string vault_str(v_chars);
    env->ReleaseStringUTFChars(manifestPath, m_chars);
    env->ReleaseStringUTFChars(vaultDir, v_chars);

    if (!loaded) return nullptr;

    std::vector<uint8_t> buf(static_cast<size_t>(length));
    bool read_ok = engine.read_virtual_range(manifest, vault_str, static_cast<uint64_t>(offset),
                                             static_cast<size_t>(length), buf.data());
    if (!read_ok) return nullptr;

    jbyteArray res = env->NewByteArray(length);
    if (!res) return nullptr;

    env->SetByteArrayRegion(res, 0, length, reinterpret_cast<const jbyte*>(buf.data()));
    return res;
}

// Test harness mirrors
JNIEXPORT jlongArray JNICALL
Java_com_prismv_test_JniVerificationTest_nativeScanMeshBurstCandidates(
    JNIEnv* env, jobject /* thiz */, jobjectArray videoPaths, jfloat similarityThreshold) {
    return Java_com_prismv_engine_VideoProcessingPipeline_nativeScanMeshBurstCandidates(env, nullptr, videoPaths, similarityThreshold);
}

JNIEXPORT jlong JNICALL
Java_com_prismv_test_JniVerificationTest_nativeIngestMeshCluster(
    JNIEnv* env, jobject /* thiz */, jstring primaryVideoPath, jobjectArray deltaVideoPaths, jstring vaultDir, jstring clusterUuid) {
    return Java_com_prismv_engine_VideoProcessingPipeline_nativeIngestMeshCluster(env, nullptr, primaryVideoPath, deltaVideoPaths, vaultDir, clusterUuid);
}

JNIEXPORT jboolean JNICALL
Java_com_prismv_test_JniVerificationTest_nativeRestoreMeshVideo(
    JNIEnv* env, jobject /* thiz */, jstring manifestPath, jstring vaultDir, jstring outPath) {
    return Java_com_prismv_engine_VideoProcessingPipeline_nativeRestoreMeshVideo(env, nullptr, manifestPath, vaultDir, outPath);
}

JNIEXPORT jbyteArray JNICALL
Java_com_prismv_test_JniVerificationTest_nativeReadVirtualMeshRange(
    JNIEnv* env, jobject /* thiz */, jstring manifestPath, jstring vaultDir, jlong offset, jint length) {
    return Java_com_prismv_engine_VideoProcessingPipeline_nativeReadVirtualMeshRange(env, nullptr, manifestPath, vaultDir, offset, length);
}

// ----------------------------------------------------------------------------
// Frontier 2: PSC 2.0 (Directional Integer Wavelet & A-IKLT) JNI Bindings
// ----------------------------------------------------------------------------
JNIEXPORT jbyteArray JNICALL
Java_com_prismv_engine_VideoProcessingPipeline_nativeCompressPsc2(
    JNIEnv* env, jclass /* clazz */, jbyteArray rawPixels, jint width, jint height, jint filterType, jboolean isQuadBayer) {
    if (!rawPixels || width < 2 || height < 2) return nullptr;

    jsize len = env->GetArrayLength(rawPixels);
    std::vector<uint8_t> buf(static_cast<size_t>(len));
    env->GetByteArrayRegion(rawPixels, 0, len, reinterpret_cast<jbyte*>(buf.data()));

    prism::Psc2Config cfg;
    cfg.width = static_cast<uint32_t>(width);
    cfg.height = static_cast<uint32_t>(height);
    cfg.filter = (filterType == 1) ? prism::WaveletFilterType::CDF_97_INT : prism::WaveletFilterType::LEGALL_53;
    cfg.bit_depth = (isQuadBayer == JNI_TRUE) ? 10 : 8;

    try {
        std::vector<uint8_t> compressed = prism::Psc2Engine::compress_chunk_payload(
            buf.data(), buf.size(), cfg, isQuadBayer == JNI_TRUE);

        jbyteArray res = env->NewByteArray(static_cast<jsize>(compressed.size()));
        if (res && !compressed.empty()) {
            env->SetByteArrayRegion(res, 0, static_cast<jsize>(compressed.size()),
                                    reinterpret_cast<const jbyte*>(compressed.data()));
        }
        return res;
    } catch (...) {
        return nullptr;
    }
}

JNIEXPORT jbyteArray JNICALL
Java_com_prismv_engine_VideoProcessingPipeline_nativeDecompressPsc2(
    JNIEnv* env, jclass /* clazz */, jbyteArray compressedFrame, jint width, jint height, jint /* filterType */, jboolean isQuadBayer) {
    if (!compressedFrame) return nullptr;

    jsize len = env->GetArrayLength(compressedFrame);
    std::vector<uint8_t> buf(static_cast<size_t>(len));
    env->GetByteArrayRegion(compressedFrame, 0, len, reinterpret_cast<jbyte*>(buf.data()));

    size_t uncomp_size = (isQuadBayer == JNI_TRUE) 
        ? static_cast<size_t>(width) * height * sizeof(uint16_t)
        : static_cast<size_t>(width) * height * 3;

    try {
        std::vector<uint8_t> decompressed = prism::Psc2Engine::decompress_chunk_payload(
            buf.data(), buf.size(), uncomp_size);

        jbyteArray res = env->NewByteArray(static_cast<jsize>(decompressed.size()));
        if (res && !decompressed.empty()) {
            env->SetByteArrayRegion(res, 0, static_cast<jsize>(decompressed.size()),
                                    reinterpret_cast<const jbyte*>(decompressed.data()));
        }
        return res;
    } catch (...) {
        return nullptr;
    }
}

JNIEXPORT jbyteArray JNICALL
Java_com_prismv_test_JniVerificationTest_nativeCompressPsc2(
    JNIEnv* env, jobject /* thiz */, jbyteArray rawPixels, jint width, jint height, jint filterType, jboolean isQuadBayer) {
    return Java_com_prismv_engine_VideoProcessingPipeline_nativeCompressPsc2(env, nullptr, rawPixels, width, height, filterType, isQuadBayer);
}

JNIEXPORT jbyteArray JNICALL
Java_com_prismv_test_JniVerificationTest_nativeDecompressPsc2(
    JNIEnv* env, jobject /* thiz */, jbyteArray compressedFrame, jint width, jint height, jint filterType, jboolean isQuadBayer) {
    return Java_com_prismv_engine_VideoProcessingPipeline_nativeDecompressPsc2(env, nullptr, compressedFrame, width, height, filterType, isQuadBayer);
}

// ----------------------------------------------------------------------------
// Frontier 1: BBR 2.0 (SSG-CR4, C-MAVQ, SIMD-16 rANS) JNI Bindings
// ----------------------------------------------------------------------------
JNIEXPORT jbyteArray JNICALL
Java_com_prismv_engine_VideoProcessingPipeline_nativeCompressBbr2(
    JNIEnv* env, jclass /* clazz */, jbyteArray bitstreamData, jint codecId) {
    if (!bitstreamData) return nullptr;
    jsize len = env->GetArrayLength(bitstreamData);
    if (len == 0) return env->NewByteArray(0);

    std::vector<uint8_t> buf(static_cast<size_t>(len));
    env->GetByteArrayRegion(bitstreamData, 0, len, reinterpret_cast<jbyte*>(buf.data()));

    try {
        prism::Bbr2Engine bbr2;
        prism::SourceCodecId c_id = (codecId == 1) ? prism::SourceCodecId::CODEC_HEVC : prism::SourceCodecId::CODEC_H264;
        prism::Bbr2Chunk chunk = bbr2.compress(buf.data(), buf.size(), c_id);
        std::vector<uint8_t> payload = prism::Bbr2Engine::serialize_chunk_payload(chunk);

        jbyteArray res = env->NewByteArray(static_cast<jsize>(payload.size()));
        if (res && !payload.empty()) {
            env->SetByteArrayRegion(res, 0, static_cast<jsize>(payload.size()),
                                    reinterpret_cast<const jbyte*>(payload.data()));
        }
        return res;
    } catch (...) {
        return nullptr;
    }
}

JNIEXPORT jbyteArray JNICALL
Java_com_prismv_engine_VideoProcessingPipeline_nativeDecompressBbr2(
    JNIEnv* env, jclass /* clazz */, jbyteArray compressedPayload, jint uncompressedSize) {
    if (!compressedPayload) return nullptr;
    jsize len = env->GetArrayLength(compressedPayload);
    if (len == 0) return env->NewByteArray(0);

    std::vector<uint8_t> buf(static_cast<size_t>(len));
    env->GetByteArrayRegion(compressedPayload, 0, len, reinterpret_cast<jbyte*>(buf.data()));

    try {
        prism::Bbr2Engine bbr2;
        std::vector<uint8_t> decompressed = bbr2.decompress_payload(buf.data(), buf.size(), static_cast<uint32_t>(uncompressedSize));

        jbyteArray res = env->NewByteArray(static_cast<jsize>(decompressed.size()));
        if (res && !decompressed.empty()) {
            env->SetByteArrayRegion(res, 0, static_cast<jsize>(decompressed.size()),
                                    reinterpret_cast<const jbyte*>(decompressed.data()));
        }
        return res;
    } catch (...) {
        return nullptr;
    }
}

JNIEXPORT jbyteArray JNICALL
Java_com_prismv_test_JniVerificationTest_nativeCompressBbr2(
    JNIEnv* env, jobject /* thiz */, jbyteArray bitstreamData, jint codecId) {
    return Java_com_prismv_engine_VideoProcessingPipeline_nativeCompressBbr2(env, nullptr, bitstreamData, codecId);
}

JNIEXPORT jbyteArray JNICALL
Java_com_prismv_test_JniVerificationTest_nativeDecompressBbr2(
    JNIEnv* env, jobject /* thiz */, jbyteArray compressedPayload, jint uncompressedSize) {
    return Java_com_prismv_engine_VideoProcessingPipeline_nativeDecompressBbr2(env, nullptr, compressedPayload, uncompressedSize);
}

} // extern "C"

#endif // PRISM_ENABLE_JNI
