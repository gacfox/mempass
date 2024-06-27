package com.gacfox.mempass;

import java.awt.AWTException;
import java.awt.Image;
import java.awt.MenuItem;
import java.awt.PopupMenu;
import java.awt.SystemTray;
import java.awt.TrayIcon;
import java.awt.event.MouseAdapter;
import java.awt.event.MouseEvent;
import java.io.IOException;

import javax.imageio.ImageIO;

import com.gacfox.mempass.util.Config;
import com.gacfox.mempass.util.DbUtil;

import javafx.application.Platform;
import javafx.stage.Stage;
import lombok.Getter;
import lombok.Setter;
import lombok.extern.slf4j.Slf4j;

/**
 * 系统托盘初始化
 *
 * @author gacfox
 */
@Slf4j
public class SystemTrayInitializr {

    private static SystemTrayInitializr self = null;
    /**
     * 系统是否支持托盘
     */
    @Getter
    private boolean supportTray = false;

    /**
     * 设置当前激活窗口
     */
    @Setter
    private Stage currentStage = null;

    /**
     * 单例模式
     *
     * @return 自身对象
     */
    public static SystemTrayInitializr getInstance() {
        if (SystemTrayInitializr.self == null) {
            SystemTrayInitializr.self = new SystemTrayInitializr();
        }
        return SystemTrayInitializr.self;
    }

    /**
     * 构造方法
     */
    public SystemTrayInitializr() {
        if (!SystemTray.isSupported() || "0".equals(Config.ENABLE_SYSTEM_TRAY)) {
            // 当前操作系统不支持托盘，或配置不开启托盘
            return;
        }

        supportTray = true;
        Platform.setImplicitExit(false);

        try {
            // javafx还未集成托盘，使用awt的托盘
            SystemTray systemTray = SystemTray.getSystemTray();
            Image icon = ImageIO.read(getClass().getResourceAsStream("/icon/trayicon.png"));
            TrayIcon trayIcon = new TrayIcon(icon);
            PopupMenu popupMenu = new PopupMenu();
            MenuItem miShow = new MenuItem("打开");
            miShow.addActionListener((ev) -> {
                showCurrentStage();
            });
            MenuItem miExit = new MenuItem("退出");
            miExit.addActionListener((ev) -> {
                DbUtil.closeConnection();
                Platform.exit();
                System.exit(0);
            });
            popupMenu.add(miShow);
            popupMenu.add(miExit);
            trayIcon.setPopupMenu(popupMenu);
            trayIcon.setToolTip("Mempass");
            trayIcon.addMouseListener(new MouseAdapter() {
                @Override
                public void mouseClicked(MouseEvent e) {
                    if (e.getButton() == MouseEvent.BUTTON1) {
                        showCurrentStage();
                    }
                }
            });
            systemTray.add(trayIcon);
        } catch (AWTException | IOException e) {
            log.error("系统异常: ", e);
        }

    }

    /**
     * 恢复窗口
     */
    private void showCurrentStage() {
        if (currentStage != null) {
            Platform.runLater(() -> {
                if (currentStage.isIconified()) {
                    currentStage.setIconified(false);
                }
                if (!currentStage.isShowing()) {
                    currentStage.show();
                }
                currentStage.toFront();
            });
        }
    }
}
