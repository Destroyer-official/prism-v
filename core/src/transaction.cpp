#include "prism/transaction.hpp"
#include "prism/bbr.hpp"
#include <filesystem>
#include <fstream>
#include <chrono>

namespace fs = std::filesystem;

namespace prism {

VerifiedTransaction::VerifiedTransaction(TransactionConfig config)
    : config_(std::move(config)), state_(TransactionState::CANDIDATE) {}

VerifiedTransaction::TransactionReceipt VerifiedTransaction::execute(
    std::function<bool(const std::string& input, const std::string& temp_out)> compression_worker,
    std::function<bool(const std::string& temp_out, std::string& error_msg)> verification_worker
) {
    auto start_time = std::chrono::high_resolution_clock::now();
    TransactionReceipt receipt;

    // Stage 1: CANDIDATE
    if (!stage_candidate()) {
        rollback("Stage CANDIDATE failed: original file not accessible or sandbox unwritable");
        receipt.final_state = state_;
        receipt.failure_reason = "Original file inaccessible: " + config_.original_media_path;
        return receipt;
    }

    // Compute original SHA-256
    try {
        std::ifstream in(config_.original_media_path, std::ios::binary);
        std::vector<uint8_t> orig_bytes((std::istreambuf_iterator<char>(in)),
                                        std::istreambuf_iterator<char>());
        receipt.original_sha256 = BbrEngine::compute_sha256(orig_bytes.data(), orig_bytes.size());
    } catch (...) {
        rollback("Failed to read original media bytes for SHA-256 computation");
        receipt.final_state = state_;
        receipt.failure_reason = "Failed to hash original media";
        return receipt;
    }

    // Run compression worker to private sandbox
    if (!compression_worker(config_.original_media_path, config_.sandbox_temp_path)) {
        rollback("Compression worker execution failed");
        receipt.final_state = state_;
        receipt.failure_reason = "Compression worker returned error";
        return receipt;
    }

    // Stage 2: LOCAL_VERIFY
    std::string verify_err;
    if (!stage_local_verify(verification_worker, verify_err)) {
        rollback("Stage LOCAL_VERIFY failed: " + verify_err);
        receipt.final_state = state_;
        receipt.failure_reason = verify_err;
        return receipt;
    }

    // Stage 3: COMMIT
    if (!stage_commit()) {
        rollback("Stage COMMIT failed");
        receipt.final_state = state_;
        receipt.failure_reason = "Commit stage failure";
        return receipt;
    }

    // Stage 4: PUBLISH (atomic staging)
    std::string pub_path;
    if (!stage_publish(pub_path)) {
        rollback("Stage PUBLISH failed");
        receipt.final_state = state_;
        receipt.failure_reason = "Publish stage failure";
        return receipt;
    }
    receipt.published_path = pub_path;

    // Stage 5: GRACE (move original to trash vault, never delete automatically)
    std::string trash_path;
    if (!stage_grace(trash_path)) {
        // Even if moving to trash fails, publication succeeded; do not destroy original
        receipt.trashed_original_path = config_.original_media_path;
    } else {
        receipt.trashed_original_path = trash_path;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    receipt.elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    receipt.success = true;
    receipt.final_state = TransactionState::GRACE;
    state_ = TransactionState::GRACE;

    return receipt;
}

bool VerifiedTransaction::stage_candidate() {
    if (!fs::exists(config_.original_media_path)) {
        return false;
    }

    // Ensure temp output directory exists
    fs::path temp_p(config_.sandbox_temp_path);
    if (temp_p.has_parent_path()) {
        fs::create_directories(temp_p.parent_path());
    }

    // Remove any stale temp file
    if (fs::exists(config_.sandbox_temp_path)) {
        fs::remove(config_.sandbox_temp_path);
    }

    state_ = TransactionState::CANDIDATE;
    return true;
}

bool VerifiedTransaction::stage_local_verify(
    std::function<bool(const std::string&, std::string&)> verifier,
    std::string& err
) {
    state_ = TransactionState::LOCAL_VERIFY;
    if (!fs::exists(config_.sandbox_temp_path)) {
        err = "Temp file does not exist after compression";
        return false;
    }

    uintmax_t temp_size = fs::file_size(config_.sandbox_temp_path);
    if (temp_size == 0) {
        err = "Temp compressed file has 0 bytes";
        return false;
    }

    return verifier(config_.sandbox_temp_path, err);
}

bool VerifiedTransaction::stage_commit() {
    state_ = TransactionState::COMMIT;
    return true;
}

bool VerifiedTransaction::stage_publish(std::string& published_path) {
    state_ = TransactionState::PUBLISH;

    // Final publication path
    fs::path orig(config_.original_media_path);
    fs::path target_pub = orig.parent_path() / (orig.stem().string() + "_prism" + orig.extension().string());

    std::error_code ec;
    // Rename atomically into place
    fs::rename(config_.sandbox_temp_path, target_pub, ec);
    if (ec) {
        // Fallback to copy + remove
        fs::copy_file(config_.sandbox_temp_path, target_pub, fs::copy_options::overwrite_existing, ec);
        if (ec) return false;
        fs::remove(config_.sandbox_temp_path, ec);
    }

    published_path = target_pub.string();
    return true;
}

bool VerifiedTransaction::stage_grace(std::string& trashed_path) {
    state_ = TransactionState::GRACE;
    if (config_.vault_trash_dir.empty()) {
        // Retain original untouched in original folder
        trashed_path = config_.original_media_path;
        return true;
    }

    fs::path trash_dir(config_.vault_trash_dir);
    fs::create_directories(trash_dir);

    fs::path orig(config_.original_media_path);
    fs::path dest = trash_dir / orig.filename();

    std::error_code ec;
    fs::rename(orig, dest, ec);
    if (!ec) {
        trashed_path = dest.string();
        return true;
    }

    trashed_path = config_.original_media_path;
    return false;
}

void VerifiedTransaction::rollback(const std::string& reason) {
    state_ = TransactionState::FAILED;
    (void)reason;
    if (fs::exists(config_.sandbox_temp_path)) {
        std::error_code ec;
        fs::remove(config_.sandbox_temp_path, ec);
    }
}

} // namespace prism
