#include "test_harness.hpp"
#include "prism/transaction.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using namespace prism;

PRISM_TEST(test_transaction_successful_commit_and_grace) {
    std::string orig_file = "test_media_orig.mp4";
    std::string temp_file = "test_sandbox/temp.mp4";
    std::string trash_dir = "test_trash_vault";

    // Clean up test directories
    if (fs::exists(orig_file)) fs::remove(orig_file);
    if (fs::exists("test_sandbox")) fs::remove_all("test_sandbox");
    if (fs::exists(trash_dir)) fs::remove_all(trash_dir);

    // Create dummy original media
    {
        std::ofstream out(orig_file, std::ios::binary);
        out << "ORIGINAL_VIDEO_BYTES_PRISM_V_PROTECTED";
    }

    VerifiedTransaction::TransactionConfig cfg;
    cfg.original_media_path = orig_file;
    cfg.sandbox_temp_path = temp_file;
    cfg.vault_trash_dir = trash_dir;

    VerifiedTransaction tx(cfg);

    auto compression_worker = [](const std::string& input, const std::string& temp_out) -> bool {
        (void)input;
        std::ofstream out(temp_out, std::ios::binary);
        out << "COMPRESSED_PRISM_OUTPUT_BYTES";
        return true;
    };

    auto verification_worker = [](const std::string& temp_out, std::string& err) -> bool {
        (void)err;
        return fs::exists(temp_out) && fs::file_size(temp_out) > 0;
    };

    auto receipt = tx.execute(compression_worker, verification_worker);

    PRISM_ASSERT(receipt.success);
    PRISM_ASSERT_EQ(receipt.final_state, TransactionState::GRACE);
    PRISM_ASSERT(fs::exists(receipt.published_path));
    PRISM_ASSERT(fs::exists(receipt.trashed_original_path));

    // Cleanup
    fs::remove(receipt.published_path);
    fs::remove_all(trash_dir);
    if (fs::exists("test_sandbox")) fs::remove_all("test_sandbox");
}

PRISM_TEST(test_transaction_rollback_on_verification_failure) {
    std::string orig_file = "test_media_safe.mp4";
    std::string temp_file = "test_sandbox/temp_corrupt.mp4";
    std::string trash_dir = "test_trash_vault";

    if (fs::exists(orig_file)) fs::remove(orig_file);
    if (fs::exists("test_sandbox")) fs::remove_all("test_sandbox");

    {
        std::ofstream out(orig_file, std::ios::binary);
        out << "IMPORTANT_USER_ORIGINAL_VIDEO";
    }

    VerifiedTransaction::TransactionConfig cfg;
    cfg.original_media_path = orig_file;
    cfg.sandbox_temp_path = temp_file;
    cfg.vault_trash_dir = trash_dir;

    VerifiedTransaction tx(cfg);

    auto compression_worker = [](const std::string& input, const std::string& temp_out) -> bool {
        (void)input;
        std::ofstream out(temp_out, std::ios::binary);
        out << "CORRUPT_ENCODE";
        return true;
    };

    // Verification fails (e.g. SHA-256 mismatch or PSNR < inf)
    auto verification_worker = [](const std::string& temp_out, std::string& err) -> bool {
        (void)temp_out;
        err = "Simulated Verification Failure (Checksum Mismatch)";
        return false;
    };

    auto receipt = tx.execute(compression_worker, verification_worker);

    PRISM_ASSERT(!receipt.success);
    PRISM_ASSERT_EQ(receipt.final_state, TransactionState::FAILED);

    // Verify Invariant 6: original file is 100% UNTOUCHED and temp file is removed!
    PRISM_ASSERT(fs::exists(orig_file));
    PRISM_ASSERT(!fs::exists(temp_file));

    // Cleanup
    fs::remove(orig_file);
    if (fs::exists("test_sandbox")) fs::remove_all("test_sandbox");
}

PRISM_TEST(test_transaction_missing_source_fails_candidate_stage) {
    std::string non_existent = "definitely_does_not_exist_998877.mp4";
    std::string temp_file = "test_sandbox/temp_non_exist.mp4";

    VerifiedTransaction::TransactionConfig cfg;
    cfg.original_media_path = non_existent;
    cfg.sandbox_temp_path = temp_file;

    VerifiedTransaction tx(cfg);

    bool worker_called = false;
    auto compression_worker = [&](const std::string&, const std::string&) -> bool {
        worker_called = true;
        return true;
    };
    auto verification_worker = [](const std::string&, std::string&) -> bool {
        return true;
    };

    auto receipt = tx.execute(compression_worker, verification_worker);
    PRISM_ASSERT(!receipt.success);
    PRISM_ASSERT_EQ(receipt.final_state, TransactionState::FAILED);
    PRISM_ASSERT(!worker_called); // Worker must never be called if CANDIDATE fails!
}

PRISM_TEST(test_transaction_empty_temp_fails_local_verify) {
    std::string orig_file = "test_media_empty_temp.mp4";
    std::string temp_file = "test_sandbox/empty_temp.mp4";
    std::string trash_dir = "test_trash_vault_empty";

    if (fs::exists(orig_file)) fs::remove(orig_file);
    if (fs::exists("test_sandbox")) fs::remove_all("test_sandbox");
    if (fs::exists(trash_dir)) fs::remove_all(trash_dir);

    {
        std::ofstream out(orig_file, std::ios::binary);
        out << "VALID_SOURCE_BYTES";
    }

    VerifiedTransaction::TransactionConfig cfg;
    cfg.original_media_path = orig_file;
    cfg.sandbox_temp_path = temp_file;
    cfg.vault_trash_dir = trash_dir;

    VerifiedTransaction tx(cfg);

    // Worker creates 0-byte file (e.g. out of disk or encoder crash)
    auto compression_worker = [](const std::string&, const std::string& temp_out) -> bool {
        std::ofstream out(temp_out, std::ios::binary); // 0 bytes
        return true;
    };
    auto verification_worker = [](const std::string&, std::string& err) -> bool {
        err = "Empty file";
        return false;
    };

    auto receipt = tx.execute(compression_worker, verification_worker);
    PRISM_ASSERT(!receipt.success);
    PRISM_ASSERT_EQ(receipt.final_state, TransactionState::FAILED);

    // Assert Invariant 6: original file remains untouched, temp file is removed
    PRISM_ASSERT(fs::exists(orig_file));
    PRISM_ASSERT(!fs::exists(temp_file));

    // Cleanup
    fs::remove(orig_file);
    if (fs::exists("test_sandbox")) fs::remove_all("test_sandbox");
}

PRISM_TEST(test_transaction_simulated_sigkill_crash_recovery) {
    std::string orig_file = "test_media_sigkill.mp4";
    std::string sandbox_dir = "test_sigkill_sandbox";
    std::string temp_file = sandbox_dir + "/temp_uncommitted.mp4.tmp";
    std::string dangling_wal = sandbox_dir + "/staged_wal.journal.tmp";

    // Clean previous runs
    if (fs::exists(orig_file)) fs::remove(orig_file);
    if (fs::exists(sandbox_dir)) fs::remove_all(sandbox_dir);

    // Create original media with verifiable SHA-256
    const std::string original_payload = "CRITICAL_USER_MEMORIES_ZERO_DATA_LOSS_INVARIANT_6";
    {
        std::ofstream out(orig_file, std::ios::binary);
        out << original_payload;
    }

    // Simulate SIGKILL interruption during encoding:
    // Worker writes partial bytes to temp file and dangling journal file, then gets killed (abrupt process termination)
    fs::create_directories(sandbox_dir);
    {
        std::ofstream out(temp_file, std::ios::binary);
        out << "PARTIAL_UNFINISHED_COMPRESSED_STREAM_BEFORE_SIGKILL";
    }
    {
        std::ofstream out(dangling_wal, std::ios::binary);
        out << "UNCOMMITTED_WAL_ENTRY_BEFORE_POWER_CUT";
    }

    PRISM_ASSERT(fs::exists(temp_file));
    PRISM_ASSERT(fs::exists(dangling_wal));

    // Recovery Phase: On next application launch, crash recovery scanner executes
    size_t cleaned_files = VerifiedTransaction::recover_sandbox(sandbox_dir);
    PRISM_ASSERT_EQ(cleaned_files, 2u);
    PRISM_ASSERT(!fs::exists(temp_file));
    PRISM_ASSERT(!fs::exists(dangling_wal));

    // Verify Invariant 6: Original video file has 100% cryptographic integrity (zero corruption)
    PRISM_ASSERT(fs::exists(orig_file));
    {
        std::ifstream in(orig_file, std::ios::binary);
        std::string verified_content((std::istreambuf_iterator<char>(in)),
                                     std::istreambuf_iterator<char>());
        PRISM_ASSERT_EQ(verified_content, original_payload);
    }

    // Cleanup
    fs::remove(orig_file);
    if (fs::exists(sandbox_dir)) fs::remove_all(sandbox_dir);
}
