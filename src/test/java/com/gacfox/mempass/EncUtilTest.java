package com.gacfox.mempass;

import com.gacfox.mempass.util.EncUtil;
import org.junit.jupiter.api.Test;

import static org.junit.jupiter.api.Assertions.assertEquals;

public class EncUtilTest {
    @Test
    public void testEncrypt() {
        String str = "Hello, world!";
        String password = "abc123";

        byte[] encData = EncUtil.pbeEncrypt(str.getBytes(), password);
        byte[] deEncData = EncUtil.pbeDecrypt(encData, password);
        String result = new String(deEncData);
        assertEquals(str, result);
    }

    @Test
    public void testEncryptStr() {
        String str = "Hello, world!";
        String password = "abc123";

        String encData = EncUtil.pbeEncryptStr(str, password);
        String result = EncUtil.pbeDecryptStr(encData, password);
        assertEquals(str, result);
    }

    @Test
    public void testBase64() {
        String str = "abc123";
        String strBase64 = EncUtil.bytesToBase64(str.getBytes());
        byte[] strBytes = EncUtil.base64ToBytes(strBase64);
        String result = new String(strBytes);
        assertEquals(str, result);
    }
}
