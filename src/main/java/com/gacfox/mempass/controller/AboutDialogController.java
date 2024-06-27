package com.gacfox.mempass.controller;

import java.io.*;
import java.nio.charset.StandardCharsets;

import javafx.fxml.FXML;
import javafx.scene.control.TextArea;
import lombok.extern.slf4j.Slf4j;

/**
 * 关于对话框
 *
 * @author gacfox
 */
@Slf4j
public class AboutDialogController {

    @FXML
    private TextArea taLicense;

    @FXML
    private void initialize() {
        try (InputStream inputStream = AboutDialogController.class.getClassLoader().getResourceAsStream("conf/LICENSE")) {
            if (inputStream != null) {
                ByteArrayOutputStream byteArrayOutputStream = new ByteArrayOutputStream();
                byte[] buffer = new byte[1024];
                int length;
                while ((length = inputStream.read(buffer)) != -1) {
                    byteArrayOutputStream.write(buffer, 0, length);
                }
                String result = byteArrayOutputStream.toString(StandardCharsets.UTF_8);
                taLicense.setText(result);
            }
        } catch (IOException e) {
            log.error("加载开源协议异常: ", e);
        }
    }
}
