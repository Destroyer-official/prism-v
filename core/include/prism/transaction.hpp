#pragma once
#ifndef PRISM_TRANSACTION_HPP
#define PRISM_TRANSACTION_HPP

#include "prism/types.hpp"
#include <string>
#include <functional>
#include <chrono>

namespace prism {

/**
 * @brief 5-Stage Verified Replacement Transaction Engine.
 *
 * Transactional file replacement with hash verification.
 * In-place destructive file overwrites are strictly forbidden.
 */
class VerifiedTransaction {
public:
    struct TransactionConfig {
        std::string original_media_path;
        std::string sandbox_temp_path;
        std::string vault_trash_dir;
        bool is_lossless = true;
        float min_vmaf_threshold = 92.0f;
    };

    struct TransactionReceipt {
        TransactionState final_state = TransactionState::CANDIDATE;
        bool success = false;
        std::string published_path;
        std::string trashed_original_path;
        std::string failure_reason;
        Sha256Digest original_sha256{};
        Sha256Digest output_sha256{};
        double elapsed_ms = 0.0;
    };

    explicit VerifiedTransaction(TransactionConfig config);
    ~VerifiedTransaction() = default;

    /**
     * @brief Scan and clean up dangling temporary files left by an abrupt SIGKILL or crash.
     * @param sandbox_dir Directory containing sandbox temporary files.
     * @return Number of cleaned dangling temporary files.
     */
    static size_t recover_sandbox(const std::string& sandbox_dir);

    /**
     * @brief Execute full 5-stage transaction with rollback on any failure.
     *
     * @param compression_worker Function executing compression to sandbox_temp_path.
     * @param verification_worker Function verifying temp file before commit.
     * @return Immutable TransactionReceipt.
     */
    TransactionReceipt execute(
        std::function<bool(const std::string& input, const std::string& temp_out)> compression_worker,
        std::function<bool(const std::string& temp_out, std::string& error_msg)> verification_worker
    );

    TransactionState current_state() const { return state_; }

private:
    TransactionConfig config_;
    TransactionState state_ = TransactionState::CANDIDATE;

    bool stage_candidate();
    bool stage_local_verify(std::function<bool(const std::string&, std::string&)> verifier, std::string& err);
    bool stage_commit();
    bool stage_publish(std::string& published_path);
    bool stage_grace(std::string& trashed_path);
    void rollback(const std::string& reason);
};

} // namespace prism

#endif // PRISM_TRANSACTION_HPP
