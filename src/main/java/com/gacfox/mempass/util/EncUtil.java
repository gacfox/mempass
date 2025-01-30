package com.gacfox.mempass.util;

import lombok.extern.slf4j.Slf4j;

import java.security.*;
import java.security.spec.InvalidKeySpecException;
import java.util.Base64;

import javax.crypto.Cipher;
import javax.crypto.SecretKeyFactory;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.PBEKeySpec;
import javax.crypto.spec.SecretKeySpec;

/**
 * 加密工具类
 *
 * @author gacfox
 */
@Slf4j
public class EncUtil {

    public static final byte[] SALT = new byte[]{2, 16, 123, 5, 87, 40, 31, 9};
    public static final int ITERATIONS = 100;
    public static final String ALGORITHM = "PBKDF2WithHmacSHA256";

    /**
     * 根据密码获取AES秘钥
     *
     * @param password 密码字符串
     * @return 秘钥
     */
    private static Key toAesKey(String password) {
        try {
            PBEKeySpec spec = new PBEKeySpec(password.toCharArray(), SALT, ITERATIONS, 128);
            SecretKeyFactory factory = SecretKeyFactory.getInstance(ALGORITHM);
            return new SecretKeySpec(factory.generateSecret(spec).getEncoded(), "AES");
        } catch (InvalidKeySpecException | NoSuchAlgorithmException e) {
            log.error("获取密钥失败: ALGORITHM [" + ALGORITHM + "]");
            throw new RuntimeException(e);
        }
    }

    /**
     * 加密
     *
     * @param str      明文
     * @param password 密码
     * @return 密文
     */
    public static String pbeEncryptStr(String str, String password) {
        byte[] strBytes = str.getBytes();
        byte[] encBytes = pbeEncrypt(strBytes, password);
        return bytesToBase64(encBytes);
    }

    /**
     * 解密
     *
     * @param str      密文
     * @param password 密码
     * @return 明文
     */
    public static String pbeDecryptStr(String str, String password) {
        byte[] encBytes = base64ToBytes(str);
        byte[] bytes = pbeDecrypt(encBytes, password);
        return new String(bytes);
    }

    /**
     * AES加密
     *
     * @param data     明文数据
     * @param password 密码
     * @return 密文数据
     */
    public static byte[] pbeEncrypt(byte[] data, String password) {
        try {
            // 密钥
            Key key = toAesKey(password);

            // 随机IV
            SecureRandom secureRandom = new SecureRandom();
            byte[] iv = new byte[16];
            secureRandom.nextBytes(iv);

            // 初始化加密器
            IvParameterSpec ivSpec = new IvParameterSpec(iv);
            Cipher cipher = Cipher.getInstance("AES/CBC/PKCS5Padding");
            cipher.init(Cipher.ENCRYPT_MODE, key, ivSpec);

            // 加密
            byte[] encryptedData = cipher.doFinal(data);

            // 组合密文
            byte[] combined = new byte[SALT.length + iv.length + encryptedData.length];
            System.arraycopy(SALT, 0, combined, 0, SALT.length);
            System.arraycopy(iv, 0, combined, SALT.length, iv.length);
            System.arraycopy(encryptedData, 0, combined, SALT.length + iv.length, encryptedData.length);

            return combined;
        } catch (Exception e) {
            log.error("AES加密失败");
            throw new RuntimeException(e);
        }
    }

    /**
     * AES解密
     *
     * @param data     密文数据
     * @param password 密码
     * @return 明文数据
     */
    public static byte[] pbeDecrypt(byte[] data, String password) {
        try {
            // 密钥
            Key key = toAesKey(password);

            // 提取密文
            byte[] iv = new byte[16];
            System.arraycopy(data, SALT.length, iv, 0, iv.length);
            byte[] encryptedData = new byte[data.length - SALT.length - iv.length];
            System.arraycopy(data, SALT.length + iv.length, encryptedData, 0, encryptedData.length);

            // 初始化解密器
            IvParameterSpec ivSpec = new IvParameterSpec(iv);
            Cipher cipher = Cipher.getInstance("AES/CBC/PKCS5Padding");
            cipher.init(Cipher.DECRYPT_MODE, key, ivSpec);

            // 解密
            return cipher.doFinal(encryptedData);
        } catch (Exception e) {
            log.error("AES解密失败");
            throw new RuntimeException(e);
        }
    }

    /**
     * 二进制编码为Base64
     *
     * @param data 二进制数据
     * @return Base64编码
     */
    public static String bytesToBase64(byte[] data) {
        return Base64.getEncoder().encodeToString(data);
    }

    /**
     * Base64编码解码为二进制数据
     *
     * @param data Base64编码
     * @return 二进制数据
     */
    public static byte[] base64ToBytes(String data) {
        return Base64.getDecoder().decode(data);
    }

    /**
     * SHA256消息摘要
     *
     * @param str 输入信息
     * @return 散列值
     */
    public static String sha256Hex(String str) {
        String result = null;
        try {
            byte[] strBytes = str.getBytes();
            byte[] combined = new byte[strBytes.length + SALT.length];
            System.arraycopy(strBytes, 0, combined, 0, strBytes.length);
            MessageDigest messageDigest = MessageDigest.getInstance("SHA-256");
            byte[] resultData = messageDigest.digest(combined);
            result = encodeHex(resultData);

        } catch (NoSuchAlgorithmException e) {
            log.error("加密失败: ", e);
        }
        return result;
    }

    /**
     * 将二进制数据编码为十六进制数字串
     *
     * @param digest 二进制数据
     * @return 十六进制数字字符串
     */
    private static String encodeHex(byte[] digest) {
        StringBuilder sb = new StringBuilder();
        for (byte b : digest) {
            sb.append(Integer.toString((b & 0xff) + 0x100, 16).substring(1));
        }
        return sb.toString();
    }
}
