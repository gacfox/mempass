package com.gacfox.mempass;

import com.gacfox.mempass.util.Config;
import com.gacfox.mempass.util.DbUtil;

import javafx.fxml.FXMLLoader;
import javafx.scene.Parent;
import javafx.scene.Scene;
import javafx.scene.image.Image;
import javafx.stage.Modality;
import javafx.stage.Stage;

/**
 * 窗口初始化
 * 
 * @author gacfox
 */
public class WindowInitializr {

	private final Image iconImage = new Image(getClass().getResourceAsStream("/icon/icon.png"));

	private static WindowInitializr self = null;

	public Stage STAGE_LOGIN = null;
	public Stage STAGE_AUTH_INIT = null;
	public Stage STAGE_MAIN = null;
	public Stage STAGE_NEW_CATEGORY_DIALOG = null;
	public Stage STAGE_NEW_ACCOUNT_DIALOG = null;
	public Stage STAGE_RANDOM_PASSWORD_DIALOG = null;
	public Stage STAGE_CHANGE_PASSWORD_DIALOG = null;
	public Stage STAGE_ABOUT_DIALOG = null;

	/**
	 * 单例模式
	 * 
	 * @return 自身对象
	 */
	public static WindowInitializr getInstance() {
		if (WindowInitializr.self == null) {
			WindowInitializr.self = new WindowInitializr();
		}
		return WindowInitializr.self;
	}

	/**
	 * 登录窗口初始化
	 * 
	 * @param primaryStage 由于是根窗口，因此需要传入primaryStage
	 * @throws Exception 异常
	 */
	public void initStageLogin(Stage primaryStage) throws Exception {
		Parent loginRoot = FXMLLoader.load(this.getClass().getResource("/fxml/Login.fxml"));
		Scene loginScene = new Scene(loginRoot);
		STAGE_LOGIN = primaryStage;
		STAGE_LOGIN.setTitle("登录");
		STAGE_LOGIN.setScene(loginScene);
		STAGE_LOGIN.getIcons().add(iconImage);
	}

	/**
	 * 系统初始化窗口初始化
	 * 
	 * @throws Exception 异常
	 */
	public void initStageAuthInit() throws Exception {
		Parent authInitRoot = FXMLLoader.load(this.getClass().getResource("/fxml/AuthInit.fxml"));
		Scene authInitScene = new Scene(authInitRoot);
		STAGE_AUTH_INIT = new Stage();
		STAGE_AUTH_INIT.initOwner(STAGE_LOGIN);
		STAGE_AUTH_INIT.setTitle("初始化");
		STAGE_AUTH_INIT.setScene(authInitScene);
		STAGE_AUTH_INIT.getIcons().add(iconImage);
	}

	/**
	 * 主窗口初始化
	 * 
	 * @throws Exception 异常
	 */
	public void initStageMain() throws Exception {
		Parent mainRoot = FXMLLoader.load(this.getClass().getResource("/fxml/Main.fxml"));
		Scene mainScene = new Scene(mainRoot);
		STAGE_MAIN = new Stage();
		STAGE_MAIN.initOwner(STAGE_LOGIN);
		STAGE_MAIN.setTitle("Mempass账号管理系统");
		STAGE_MAIN.setScene(mainScene);
		STAGE_MAIN.getIcons().add(iconImage);

		if (!SystemTrayInitializr.getInstance().isSupportTray() || "0".equals(Config.ENABLE_SYSTEM_TRAY)) {
			// 不支持系统托盘的操作系统，或是配置不开启托盘，主窗口关闭时尝试清理连接准备退出程序
			STAGE_MAIN.setOnCloseRequest((ev) -> {
				DbUtil.closeConnection();
			});
		}
	}

	/**
	 * 新建分类窗口初始化
	 * 
	 * @throws Exception 异常
	 */
	public void initStageNewCategoryDialog() throws Exception {
		Parent newCategoryDialogRoot = FXMLLoader.load(this.getClass().getResource("/fxml/NewCategoryDialog.fxml"));
		Scene newCategoryDialogScene = new Scene(newCategoryDialogRoot);
		STAGE_NEW_CATEGORY_DIALOG = new Stage();
		STAGE_NEW_CATEGORY_DIALOG.initOwner(STAGE_MAIN);
		STAGE_NEW_CATEGORY_DIALOG.initModality(Modality.WINDOW_MODAL);
		STAGE_NEW_CATEGORY_DIALOG.setTitle("添加分类");
		STAGE_NEW_CATEGORY_DIALOG.setScene(newCategoryDialogScene);
		STAGE_NEW_CATEGORY_DIALOG.getIcons().add(iconImage);
	}

	/**
	 * 新建账户项窗口初始化
	 * 
	 * @throws Exception 异常
	 */
	public void initStageNewAccountDialog() throws Exception {
		Parent newAccountDialogRoot = FXMLLoader.load(this.getClass().getResource("/fxml/NewAccountDialog.fxml"));
		Scene newAccountDialogScene = new Scene(newAccountDialogRoot);
		STAGE_NEW_ACCOUNT_DIALOG = new Stage();
		STAGE_NEW_ACCOUNT_DIALOG.initOwner(STAGE_MAIN);
		STAGE_NEW_ACCOUNT_DIALOG.initModality(Modality.WINDOW_MODAL);
		STAGE_NEW_ACCOUNT_DIALOG.setTitle("添加账户项");
		STAGE_NEW_ACCOUNT_DIALOG.setScene(newAccountDialogScene);
		STAGE_NEW_ACCOUNT_DIALOG.getIcons().add(iconImage);
	}

	/**
	 * 随机密码窗口初始化
	 * 
	 * @throws Exception 异常
	 */
	public void initStageRandomPasswordDialog() throws Exception {
		Parent randomPasswordDialogRoot = FXMLLoader.load(this.getClass().getResource("/fxml/RandomPasswordDialog.fxml"));
		Scene randomPasswordDialogScene = new Scene(randomPasswordDialogRoot);
		STAGE_RANDOM_PASSWORD_DIALOG = new Stage();
		STAGE_RANDOM_PASSWORD_DIALOG.initOwner(STAGE_MAIN);
		STAGE_RANDOM_PASSWORD_DIALOG.initModality(Modality.WINDOW_MODAL);
		STAGE_RANDOM_PASSWORD_DIALOG.setTitle("随机密码生成");
		STAGE_RANDOM_PASSWORD_DIALOG.setScene(randomPasswordDialogScene);
		STAGE_RANDOM_PASSWORD_DIALOG.getIcons().add(iconImage);
	}

	/**
	 * 修改密码窗口初始化
	 * 
	 * @throws Exception 异常
	 */
	public void initStageChangePasswordDialog() throws Exception {
		Parent changePasswordDialogRoot = FXMLLoader.load(this.getClass().getResource("/fxml/ChangePasswordDialog.fxml"));
		Scene changePasswordDialogScene = new Scene(changePasswordDialogRoot);
		STAGE_CHANGE_PASSWORD_DIALOG = new Stage();
		STAGE_CHANGE_PASSWORD_DIALOG.initOwner(STAGE_MAIN);
		STAGE_CHANGE_PASSWORD_DIALOG.initModality(Modality.WINDOW_MODAL);
		STAGE_CHANGE_PASSWORD_DIALOG.setTitle("修改密码");
		STAGE_CHANGE_PASSWORD_DIALOG.setScene(changePasswordDialogScene);
		STAGE_CHANGE_PASSWORD_DIALOG.getIcons().add(iconImage);
	}

	/**
	 * 关于页面
	 * 
	 * @throws Exception 异常
	 */
	public void initStageAboutDialog() throws Exception {
		Parent aboutDialogRoot = FXMLLoader.load(this.getClass().getResource("/fxml/AboutDialog.fxml"));
		Scene aboutDialogScene = new Scene(aboutDialogRoot);
		STAGE_ABOUT_DIALOG = new Stage();
		STAGE_ABOUT_DIALOG.initOwner(STAGE_MAIN);
		STAGE_ABOUT_DIALOG.initModality(Modality.WINDOW_MODAL);
		STAGE_ABOUT_DIALOG.setTitle("关于");
		STAGE_ABOUT_DIALOG.setScene(aboutDialogScene);
		STAGE_ABOUT_DIALOG.getIcons().add(iconImage);
	}
}
