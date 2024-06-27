package com.gacfox.mempass.util;

import java.io.File;

/**
 * 静态配置常量
 *
 * @author gacfox
 */
public class Config {
    /**
     * 工作路径
     */
    public static String WORK_DIR;
    /**
     * 认证ID，默认default仅供测试，应用必须设置一个AUTH_ID才能继续后续步骤
     */
    public static String AUTH_ID = "default";

    /**
     * 认证口令，默认admin123仅供测试，应用必须设置一个AUTH_KEY才能继续后续步骤
     */
    public static String AUTH_KEY = "admin123";

    /**
     * 上次登录的认证ID
     */
    public static String LAST_AUTH_ID = null;

    /**
     * H2数据库的Trace级别，默认关闭
     */
    public static String H2_TRACE = "0";

    /**
     * 是否启用托盘，启用后关闭主界面不会实际退出，而是停留在系统托盘中
     */
    public static String ENABLE_SYSTEM_TRAY = "0";

    static {
        String appDataDir;
        String osName = System.getProperty("os.name");
        if (osName.toLowerCase().contains("windows")) {
            // Windows系统使用~/AppData/Roaming/mempass路径
            appDataDir = System.getenv("APPDATA");
            WORK_DIR = appDataDir + "/mempass";
        } else {
            // Unix/Linux使用~/.mempass路径
            appDataDir = System.getProperty("user.home");
            WORK_DIR = appDataDir + "/.mempass";
        }
        // 工作路径不存在则创建
        File workDirFile = new java.io.File(WORK_DIR);
        if (!workDirFile.exists()) {
            boolean ignored = workDirFile.mkdir();
        }
    }
}
