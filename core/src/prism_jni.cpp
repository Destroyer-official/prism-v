#include "prism/types.hpp"
#include "prism/detint.hpp"
#include "prism/bbr.hpp"
#include "prism/psc.hpp"
#include "prism/coolchic.hpp"
#include "prism/afgs1.hpp"
#include "prism/container_pvloss.hpp"
#include "prism/isobmff.hpp"
#include "prism/vfs_proxy.hpp"
#include "prism/remux.hpp"
#include "prism/router.hpp"

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

    std::vector<uint8_t> compressed_payload = reader.read_chunk_payload(*entry);
    reader.close();
    env->ReleaseStringUTFChars(pvlossPath, path_chars);

    prism::BbrEngine bbr;
    prism::BbrChunk chunk;
    chunk.uncompressed_size = entry->uncompressed_size;
    chunk.rans_payload = std::move(compressed_payload);

    std::vector<uint8_t> decompressed = bbr.decompress(chunk);

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

                        // Compress GOP segment via BBR
                        prism::BbrEngine bbr;
                        prism::BbrChunk chunk = bbr.compress(gop_data.data(), gop_data.size(), prism::SourceCodecId::CODEC_H264);

                        // Write atomic GOP output
                        std::ofstream out(out_str, std::ios::binary | std::ios::trunc);
                        if (out.is_open()) {
                            out.write(reinterpret_cast<const char*>(chunk.rans_payload.data()), chunk.rans_payload.size());
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

} // extern "C"

#endif // PRISM_ENABLE_JNI
