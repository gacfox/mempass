package com.gacfox.mempass.controller;

import com.gacfox.mempass.WindowInitializr;
import com.gacfox.mempass.dao.AccountDao;
import com.gacfox.mempass.dao.AuthDao;
import com.gacfox.mempass.util.Config;
import com.gacfox.mempass.util.DbUtil;

import javafx.fxml.FXML;
import javafx.scene.control.Alert;
import javafx.scene.control.PasswordField;
import javafx.scene.control.Alert.AlertType;

/**
 * 修改密码对话框
 *
 * @author gacfox
 */
public class ChangePasswordDialogController {

    @FXML
    private PasswordField pfOldPassword;
    @FXML
    private PasswordField pfPassword;
    @FXML
    private PasswordField pfRePassword;

    private final WindowInitializr windowInitializr = WindowInitializr.getInstance();

    private final AuthDao authDao = AuthDao.getInstance();
    private final AccountDao accountDao = AccountDao.getInstance();

    /**
     * 确认按钮
     */
    public void handleConfirmButton() {

        String oldPassword = pfOldPassword.getText();
        String password = pfPassword.getText();
        String rePassword = pfPassword.getText();

        // 表单校验
        if ("".equals(oldPassword) || "".equals(password) || "".equals(rePassword)) {
            Alert alert = new Alert(AlertType.WARNING);
            alert.setTitle("警告");
            alert.setContentText("字段值不能为空！");
            alert.show();
            return;
        }
        if (!oldPassword.equals(Config.AUTH_KEY)) {
            Alert alert = new Alert(AlertType.WARNING);
            alert.setTitle("警告");
            alert.setContentText("原密码错误！");
            alert.show();
            return;
        }
        if (!password.equals(rePassword)) {
            Alert alert = new Alert(AlertType.WARNING);
            alert.setTitle("警告");
            alert.setContentText("两次新密码输入不同！");
            alert.show();
            return;
        }
        if (oldPassword.equals(password)) {
            Alert alert = new Alert(AlertType.WARNING);
            alert.setTitle("警告");
            alert.setContentText("原密码和新密码不能相同！");
            alert.show();
            return;
        }
        // 修改密码
        authDao.changePassword(password);
        // 修改字段加密
        accountDao.updateEncKey(oldPassword, password);
        // 修改整库加密
        DbUtil.changeFileEncPassword(password);
        Config.AUTH_KEY = password;

        windowInitializr.STAGE_CHANGE_PASSWORD_DIALOG.hide();
    }

    /**
     * 取消按钮
     */
    public void handleCancelButton() {
        windowInitializr.STAGE_CHANGE_PASSWORD_DIALOG.hide();
    }
}
