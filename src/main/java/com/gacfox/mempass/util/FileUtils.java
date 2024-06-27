package com.gacfox.mempass.util;

import lombok.extern.slf4j.Slf4j;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.channels.FileChannel;

/**
 * 文件操作工具类
 *
 * @author gacfox
 */
@Slf4j
public class FileUtils {

    private static final long FILE_COPY_BUFFER_SIZE = 1024 * 1024;

    /**
     * 参考apache commons-io的拷贝文件，即使源文件被操作系统锁定也执行拷贝，java.nio.file.Files.copy()做不到这点
     *
     * @param srcFile  源文件
     * @param destFile 目标文件
     */
    public static void copyFile(File srcFile, File destFile) {

        FileInputStream fis = null;
        FileChannel input = null;
        FileOutputStream fos = null;
        FileChannel output = null;

        try {

            fis = new FileInputStream(srcFile);
            input = fis.getChannel();
            fos = new FileOutputStream(destFile);
            output = fos.getChannel();

            final long size = input.size();
            long pos = 0;
            long count;
            while (pos < size) {
                final long remain = size - pos;
                count = Math.min(remain, FILE_COPY_BUFFER_SIZE);
                final long bytesCopied = output.transferFrom(input, pos, count);
                if (bytesCopied == 0) {
                    break;
                }
                pos += bytesCopied;
            }
            boolean ignored = destFile.setLastModified(srcFile.lastModified());
        } catch (IOException e) {
            log.error("IO异常: ", e);
        } finally {
            if (input != null) {
                try {
                    input.close();
                } catch (IOException e) {
                    log.error("IO异常: ", e);
                }
            }
            if (fis != null) {
                try {
                    fis.close();
                } catch (IOException e) {
                    log.error("IO异常: ", e);
                }
            }
            if (output != null) {
                try {
                    output.close();
                } catch (IOException e) {
                    log.error("IO异常: ", e);
                }
            }
            if (fos != null) {
                try {
                    fos.close();
                } catch (IOException e) {
                    log.error("IO异常: ", e);
                }
            }
        }

    }
}
