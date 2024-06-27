package com.gacfox.mempass.controller;

import java.util.Random;

import javafx.fxml.FXML;
import javafx.scene.control.ChoiceBox;
import javafx.scene.control.Spinner;
import javafx.scene.control.TextField;

/**
 * 随机密码生成对话框
 *
 * @author gacfox
 */
public class RandPasswordDialogController {

    @FXML
    private TextField tfResult;
    @FXML
    private Spinner<Integer> spCount;
    @FXML
    private ChoiceBox<String> cbCharSpace;

    public void handleGenerateButton() {
        String numberChars = "1234567890";
        String lowerCaseAlphabetChars = "abcdefghijklmnopqrstuvwxyz";
        String upperCaseAlphabetChars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        String symbolChars = "!@#$%^&*";

        StringBuilder charsBuilder = new StringBuilder();
        switch (cbCharSpace.getSelectionModel().getSelectedItem()) {
            case "仅数字":
                charsBuilder.append(numberChars);
                break;
            case "数字和小写字母":
                charsBuilder.append(numberChars);
                charsBuilder.append(lowerCaseAlphabetChars);
                break;
            case "数字和字母":
                charsBuilder.append(numberChars);
                charsBuilder.append(lowerCaseAlphabetChars);
                charsBuilder.append(upperCaseAlphabetChars);
                break;
            case "数字字母和符号":
                charsBuilder.append(numberChars);
                charsBuilder.append(lowerCaseAlphabetChars);
                charsBuilder.append(upperCaseAlphabetChars);
                charsBuilder.append(symbolChars);
                break;
            default:
                charsBuilder.append(numberChars);
                break;
        }
        char[] chars = charsBuilder.toString().toCharArray();

        StringBuilder targetBuilder = new StringBuilder();
        int targetLength = spCount.getValue();
        int charsLength = chars.length;
        Random random = new Random();
        for (int i = 0; i < targetLength; i++) {
            char randChar = chars[random.nextInt(charsLength)];
            targetBuilder.append(randChar);
        }

        String target = targetBuilder.toString();
        tfResult.setText(target);
    }

    @FXML
    private void initialize() {
        cbCharSpace.getItems().add("数字字母和符号");
        cbCharSpace.getItems().add("数字和字母");
        cbCharSpace.getItems().add("数字和小写字母");
        cbCharSpace.getItems().add("仅数字");
        cbCharSpace.getSelectionModel().selectFirst();
    }
}
