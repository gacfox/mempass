package com.gacfox.mempass.util;

import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Statement;

import lombok.extern.slf4j.Slf4j;
import org.h2.jdbc.JdbcSQLInvalidAuthorizationSpecException;
import org.h2.jdbc.JdbcSQLNonTransientConnectionException;
import org.h2.tools.ChangeFileEncryption;

import javafx.scene.control.Alert;
import javafx.scene.control.Alert.AlertType;

/**
 * 数据库操作工具类
 *
 * @author gacfox
 */
@Slf4j
public class DbUtil {

    /**
     * 数据库连接对象，该程序的DAO操作都是阻塞单线程的，所以这样复用连接没有问题
     */
    private static Connection conn = null;

    static {
        try {
            Class.forName("org.h2.Driver");
        } catch (ClassNotFoundException e) {
            log.error("数据库初始化异常: ", e);
        }
    }

    /**
     * 获取一个H2数据库连接
     *
     * @return 连接对象
     */
    public static Connection getConnection() {
        if (conn != null) {
            return conn;
        }
        try {
            String url = "jdbc:h2:" + Config.WORK_DIR + "/data/" + Config.AUTH_ID + "/mempass;TRACE_LEVEL_FILE=" + Config.H2_TRACE + ";CIPHER=AES;TRACE_LEVEL_SYSTEM_OUT=" + Config.H2_TRACE;
            String user = "sa";
            // H2的密码格式为 加密密码<空格>用户密码，这里使用嵌入式模式不需要用户密码，因此留了一个空格
            String password = Config.AUTH_KEY + " ";
            conn = DriverManager.getConnection(url, user, password);
        } catch (JdbcSQLNonTransientConnectionException e) {
            // 这里H2本身抛出的异常类型似乎有点bug
            // 解密异常可能是JdbcSQLNonTransientConnectionException
            // 也可能是JdbcSQLInvalidAuthorizationSpecException
            // 所以额外判断了一下
            if (e.getMessage().startsWith("Encryption error")) {
                Alert alert = new Alert(AlertType.WARNING);
                alert.setTitle("警告");
                alert.setContentText("数据库文件解密认证失败！该行为会报告给管理员！");
                alert.show();
                log.error("数据库初始化异常: ", e);
                throw new RuntimeException("数据库文件解密认证失败");
            } else {
                Alert alert = new Alert(AlertType.WARNING);
                alert.setTitle("警告");
                alert.setContentText("数据库占用中，同一数据库只能加载一个程序实例！");
                alert.show();
                log.error("数据库初始化异常: ", e);
                throw new RuntimeException("H2数据库文件已锁定");
            }
        } catch (JdbcSQLInvalidAuthorizationSpecException e) {
            Alert alert = new Alert(AlertType.WARNING);
            alert.setTitle("警告");
            alert.setContentText("数据库文件解密认证失败！该行为会报告给管理员！");
            alert.show();
            log.error("数据库初始化异常: ", e);
            throw new RuntimeException("数据库文件解密认证失败");
        } catch (SQLException e) {
            Alert alert = new Alert(AlertType.WARNING);
            alert.setTitle("警告");
            alert.setContentText("打开数据库连接出错！");
            alert.show();
            log.error("数据库初始化异常: ", e);
            throw new RuntimeException("获取数据库连接失败");
        }
        return conn;
    }

    /**
     * 更改数据库文件的加密密码
     */
    public static void changeFileEncPassword(String password) {

        // 断开连接
        closeConnection();

        String dir = Config.WORK_DIR + "/data/" + Config.AUTH_ID;
        String db = "mempass";
        String cipher = "AES";
        char[] decryptPassword = Config.AUTH_KEY.toCharArray();
        char[] encryptPassword = password.toCharArray();

        try {
            ChangeFileEncryption.execute(dir, db, cipher, decryptPassword, encryptPassword, false);
        } catch (SQLException e) {
            log.error("数据库操作异常: ", e);
            throw new RuntimeException("更改数据库文件的加密密码出错");
        }
    }

    /**
     * 关闭资源
     *
     * @param preparedStatement 预编译SQL
     */
    public static void closeResource(PreparedStatement preparedStatement) {
        if (preparedStatement != null) {
            try {
                preparedStatement.close();
            } catch (SQLException e) {
                log.error("数据库操作异常: ", e);
            }
        }
    }

    /**
     * 关闭资源
     *
     * @param statement SQL
     */
    public static void closeResource(Statement statement) {
        if (statement != null) {
            try {
                statement.close();
            } catch (SQLException e) {
                log.error("数据库操作异常: ", e);
            }
        }
    }

    /**
     * 关闭资源
     *
     * @param preparedStatement 预编译SQL
     * @param resultSet         结果集
     */
    public static void closeResource(PreparedStatement preparedStatement, ResultSet resultSet) {
        if (preparedStatement != null) {
            try {
                preparedStatement.close();
            } catch (SQLException e) {
                log.error("数据库操作异常: ", e);
            }
        }
        if (resultSet != null) {
            try {
                resultSet.close();
            } catch (SQLException e) {
                log.error("数据库操作异常: ", e);
            }
        }
    }

    public static void closeConnection() {
        if (conn != null) {
            try {
                conn.close();
            } catch (SQLException e) {
                log.error("数据库操作异常: ", e);
            } finally {
                conn = null;
            }
        }
    }
}
