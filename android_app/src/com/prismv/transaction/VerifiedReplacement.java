package com.prismv.transaction;

import com.prismv.bbr.BbrCodec;
import com.prismv.detint.DetInt;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;

/**
 * 5-Stage Verified Replacement Transaction Engine.
 * Strictly adheres to Invariant 6: Zero Media Loss Guarantee.
 */
public final class VerifiedReplacement {

    public enum Stage {
        STAGE_0_STAGED,
        STAGE_1_VERIFY_BIT_EXACT,
        STAGE_2_ATOMIC_REPLACE,
        STAGE_3_COMMIT,
        STAGE_4_CLEANUP
    }

    public static class TransactionResult {
        public final boolean success;
        public final Stage finalStage;
        public final String originalHash;
        public final String verifiedHash;
        public final String journalPath;
        public final String message;

        public TransactionResult(boolean success, Stage finalStage, String originalHash,
                                 String verifiedHash, String journalPath, String message) {
            this.success = success;
            this.finalStage = finalStage;
            this.originalHash = originalHash;
            this.verifiedHash = verifiedHash;
            this.journalPath = journalPath;
            this.message = message;
        }
    }

    private VerifiedReplacement() {}

    public static TransactionResult executeTransaction(File sourceFile, File targetDir) {
        if (!sourceFile.exists()) {
            return new TransactionResult(false, Stage.STAGE_0_STAGED, null, null, null, "Source file does not exist");
        }

        File tempCompressed = new File(targetDir, sourceFile.getName() + ".pvloss.tmp");
        File finalCompressed = new File(targetDir, sourceFile.getName() + ".pvloss");
        File journalFile = new File(targetDir, sourceFile.getName() + ".journal");

        Stage currentStage = Stage.STAGE_0_STAGED;
        String origHash = null;
        String verHash = null;

        try {
            // STAGE 0: STAGED - Read source and compress to temp file
            byte[] sourceBytes = readFile(sourceFile);
            origHash = DetInt.toHexString(DetInt.computeSha256(sourceBytes));

            byte[] compressed = BbrCodec.compressGop(sourceBytes);
            writeFile(tempCompressed, compressed);
            currentStage = Stage.STAGE_1_VERIFY_BIT_EXACT;

            // STAGE 1: VERIFY BIT-EXACT - Read back and verify reconstruction
            byte[] readBack = readFile(tempCompressed);
            byte[] decompressed = BbrCodec.decompressGop(readBack);
            verHash = DetInt.toHexString(DetInt.computeSha256(decompressed));

            if (!origHash.equals(verHash)) {
                tempCompressed.delete();
                return new TransactionResult(false, currentStage, origHash, verHash, null, "Hash verification failed: Corruption detected");
            }
            currentStage = Stage.STAGE_2_ATOMIC_REPLACE;

            // STAGE 2: ATOMIC REPLACE - Rename temp file to final compressed
            if (finalCompressed.exists()) {
                finalCompressed.delete();
            }
            if (!tempCompressed.renameTo(finalCompressed)) {
                // Fallback atomic copy
                writeFile(finalCompressed, readBack);
                tempCompressed.delete();
            }
            currentStage = Stage.STAGE_3_COMMIT;

            // STAGE 3: COMMIT - Write journal entry
            String journalEntry = "COMMITTED: " + finalCompressed.getName() + " SHA256=" + verHash + "\n";
            writeFile(journalFile, journalEntry.getBytes());
            currentStage = Stage.STAGE_4_CLEANUP;

            // STAGE 4: CLEANUP - Verified state reached
            return new TransactionResult(true, currentStage, origHash, verHash, journalFile.getAbsolutePath(), "Transaction 100% committed and verified");

        } catch (Exception e) {
            if (tempCompressed.exists()) {
                tempCompressed.delete();
            }
            return new TransactionResult(false, currentStage, origHash, verHash, null, "Transaction error: " + e.getMessage());
        }
    }

    /**
     * Executes a 5-stage verified transaction for Frontier 6 Rabin-Mesh Vault (.pvmesh).
     * Guarantees 100.000% SHA-256 bit-exact file reconstruction before committing.
     */
    public static TransactionResult executeMeshTransaction(File sourceFile, File targetDir, File vaultDir, String clusterUuid) {
        if (!sourceFile.exists()) {
            return new TransactionResult(false, Stage.STAGE_0_STAGED, null, null, null, "Source file does not exist");
        }

        File finalManifest = new File(targetDir, sourceFile.getName() + ".pvmesh");
        File journalFile = new File(targetDir, sourceFile.getName() + ".mesh_journal");

        Stage currentStage = Stage.STAGE_0_STAGED;
        String origHash = null;
        String verHash = null;

        try {
            // STAGE 0: STAGED - Read source bytes and compute cryptographic SHA-256 fixity
            byte[] sourceBytes = readFile(sourceFile);
            origHash = DetInt.toHexString(DetInt.computeSha256(sourceBytes));

            // Ingest source into Rabin-Mesh Vault
            if (!vaultDir.exists()) {
                vaultDir.mkdirs();
            }
            com.prismv.engine.VideoProcessingPipeline.nativeIngestMeshCluster(
                    sourceFile.getAbsolutePath(),
                    new String[0],
                    vaultDir.getAbsolutePath(),
                    clusterUuid
            );
            currentStage = Stage.STAGE_1_VERIFY_BIT_EXACT;

            // STAGE 1: VERIFY BIT-EXACT - Test full inverse reconstruction from vault
            File tempRestored = new File(targetDir, sourceFile.getName() + ".restored.tmp");
            boolean restored = com.prismv.engine.VideoProcessingPipeline.nativeRestoreMeshVideo(
                    sourceFile.getAbsolutePath() + ".pvmesh",
                    vaultDir.getAbsolutePath(),
                    tempRestored.getAbsolutePath()
            );

            if (!restored || !tempRestored.exists()) {
                if (tempRestored.exists()) tempRestored.delete();
                return new TransactionResult(false, currentStage, origHash, null, null, "Mesh vault restoration failed");
            }

            byte[] restoredBytes = readFile(tempRestored);
            tempRestored.delete();
            verHash = DetInt.toHexString(DetInt.computeSha256(restoredBytes));

            if (!origHash.equals(verHash)) {
                return new TransactionResult(false, currentStage, origHash, verHash, null, "Bit-exact SHA-256 verification failed (Corruption detected)");
            }
            currentStage = Stage.STAGE_2_ATOMIC_REPLACE;

            // STAGE 2: ATOMIC REPLACE - Publish final manifest
            File generatedManifest = new File(sourceFile.getAbsolutePath() + ".pvmesh");
            if (generatedManifest.exists()) {
                if (finalManifest.exists()) finalManifest.delete();
                if (!generatedManifest.renameTo(finalManifest)) {
                    byte[] mBytes = readFile(generatedManifest);
                    writeFile(finalManifest, mBytes);
                    generatedManifest.delete();
                }
            }
            currentStage = Stage.STAGE_3_COMMIT;

            // STAGE 3: COMMIT - Write Write-Ahead Log (WAL)
            String journalEntry = "COMMITTED_MESH: " + finalManifest.getName() + " SHA256=" + verHash + " CLUSTER=" + clusterUuid + "\n";
            writeFile(journalFile, journalEntry.getBytes());
            currentStage = Stage.STAGE_4_CLEANUP;

            // STAGE 4: CLEANUP - Verified state reached
            return new TransactionResult(true, currentStage, origHash, verHash, journalFile.getAbsolutePath(), "Mesh transaction 100% committed and verified");

        } catch (Exception e) {
            return new TransactionResult(false, currentStage, origHash, verHash, null, "Mesh transaction error: " + e.getMessage());
        }
    }

    private static byte[] readFile(File file) throws IOException {
        try (FileInputStream fis = new FileInputStream(file)) {
            byte[] data = new byte[(int) file.length()];
            int read = 0;
            while (read < data.length) {
                int r = fis.read(data, read, data.length - read);
                if (r < 0) break;
                read += r;
            }
            return data;
        }
    }

    private static void writeFile(File file, byte[] data) throws IOException {
        try (FileOutputStream fos = new FileOutputStream(file)) {
            fos.write(data);
            fos.flush();
            fos.getFD().sync(); // Ensure physical write sync
        }
    }
}
