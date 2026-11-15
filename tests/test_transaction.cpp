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
