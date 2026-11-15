import com.prismv.rans.Rans8Codec;
import java.util.*;

public class RunTest {
    public static void main(String[] args) {
        byte[] structured = new byte[16384];
        for (int i = 0; i < structured.length; i++) {
            structured[i] = (byte) ((i / 64) & 0xFF);
        }
        byte[] residuals = new byte[structured.length];
        residuals[0] = structured[0];
        for (int i = 1; i < structured.length; i++) {
            residuals[i] = (byte) (((structured[i] & 0xFF) - (structured[i - 1] & 0xFF)) & 0xFF);
        }

        List<Rans8Codec.RansSymbol> stats = Rans8Codec.buildStats(residuals);
        int[] cdfLut = Rans8Codec.buildCdfLut(stats);

        // Let's check when encoding lane 0:
        // symbols at indices 0, 8, 16, ..., 16376, 16384
        // In encode, symbols are processed backwards: from 16384 down to 0
        // Let's trace lane 0 state changes!
        System.out.println("Residual at 200 is: " + (residuals[200] & 0xFF));
        System.out.println("Residual at 208 is: " + (residuals[208] & 0xFF));
        System.out.println("Residual at 192 is: " + (residuals[192] & 0xFF));
    }
}
