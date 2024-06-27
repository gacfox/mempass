package com.gacfox.mempass.controller;

import com.gacfox.mempass.SystemTrayInitializr;
import com.gacfox.mempass.WindowInitializr;
import com.gacfox.mempass.dao.AuthDao;
import com.gacfox.mempass.util.Config;
import com.gacfox.mempass.util.PropertiesUtil;

import javafx.fxml.FXML;
import javafx.scene.control.Alert;
import javafx.scene.control.PasswordField;
import javafx.scene.control.TextField;
import javafx.scene.control.Alert.AlertType;
import lombok.extern.slf4j.Slf4j;

/**
 * 登录对话框
 *
 * @author gacfox
 */
@Slf4j
public class LoginController {

    @FXML
    private TextField tfAuthId;
    @FXML
    private PasswordField pfAuthKey;

    private final WindowInitializr windowInitializr = WindowInitializr.getInstance();
    private final AuthDao authDao = AuthDao.getInstance();

    private static LoginController self = null;

    public static LoginController getInstance() {
        return LoginController.self;
    }

    @FXML
    public void initialize() {
        if (Config.LAST_AUTH_ID != null) {
            tfAuthId.setText(Config.LAST_AUTH_ID);
        }
        LoginController.self = this;
    }

    public void clearTfAuthKey() {
        pfAuthKey.clear();
    }

    public void handleLoginButton() {
        String authId = tfAuthId.getText();
        String authKey = pfAuthKey.getText();

        // 表单校验
        if ("".equals(authId) || "".equals(authKey)) {
            Alert alert = new Alert(AlertType.WARNING);
            alert.setTitle("警告");
            alert.setContentText("字段值不能为空！");
            alert.show();
            return;
        }

        Config.AUTH_ID = authId;
        Config.AUTH_KEY = authKey;
        Config.LAST_AUTH_ID = authId;
        boolean checked = authDao.checkAuthInfo(authId, authKey);
        if (checked) {
            // 登陆成功
            windowInitializr.STAGE_LOGIN.hide();

            try {
                PropertiesUtil.saveRuntimeProperties();
                windowInitializr.initStageMain();
                windowInitializr.STAGE_MAIN.show();
                SystemTrayInitializr.getInstance().setCurrentStage(windowInitializr.STAGE_MAIN);
            } catch (Exception e) {
                log.error("登录异常: ", e);
            }
        } else {
            // 登录失败
            Alert alert = new Alert(AlertType.WARNING);
            alert.setTitle("警告");
            alert.setContentText("认证失败！该行为会报告给管理员！");
            alert.show();
        }
    }

    public void handleResetButton() {
        tfAuthId.setText("");
        pfAuthKey.setText("");
    }

    public void handleNewButton() {
        windowInitializr.STAGE_LOGIN.hide();
        windowInitializr.STAGE_AUTH_INIT.show();
        SystemTrayInitializr.getInstance().setCurrentStage(windowInitializr.STAGE_AUTH_INIT);
    }
}
