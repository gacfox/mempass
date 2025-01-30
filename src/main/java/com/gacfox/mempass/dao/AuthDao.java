package com.gacfox.mempass.dao;

import java.io.File;
import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;

import com.gacfox.mempass.util.Config;
import com.gacfox.mempass.util.DbUtil;
import com.gacfox.mempass.util.EncUtil;
import lombok.extern.slf4j.Slf4j;

/**
 * 认证操作
 *
 * @author gacfox
 */
@Slf4j
public class AuthDao {

    private static AuthDao self = null;

    /**
     * 单例模式
     *
     * @return 单例自身对象
     */
    public static AuthDao getInstance() {
        if (self == null) {
            self = new AuthDao();
        }
        return self;
    }

    /**
     * 初始化认证信息
     *
     * @param authId  认证ID（用户名）
     * @param authKey 认证口令（密码）
     */
    public void initAuthInfo(String authId, String authKey) {
        Connection conn = DbUtil.getConnection();
        PreparedStatement pstmt1 = null;
        PreparedStatement pstmt2 = null;
        String sql1 = "insert into t_conf (conf_key, conf_value) values ('auth_id', ?);";
        String sql2 = "insert into t_conf (conf_key, conf_value) values ('auth_key', ?);";
        try {
            pstmt1 = conn.prepareStatement(sql1);
            pstmt1.setString(1, authId);
            pstmt1.execute();
            pstmt2 = conn.prepareStatement(sql2);
            pstmt2.setString(1, EncUtil.sha256Hex(authKey));
            pstmt2.execute();
        } catch (SQLException e) {
            log.error("数据库操作异常: ", e);
        } finally {
            DbUtil.closeResource(pstmt1);
            DbUtil.closeResource(pstmt2);
            DbUtil.closeConnection();
        }
    }

    /**
     * 检查登入信息是否正确
     *
     * @param authId  认证ID
     * @param authKey 认证秘钥
     * @return 正确返回true，允许登入
     */
    public boolean checkAuthInfo(String authId, String authKey) {

        // 先检查是否有该用户的数据路径，没有说明未注册
        String userDataPath = Config.WORK_DIR + "/data/" + authId;
        File file = new File(userDataPath);
        if (!file.exists()) {
            return false;
        }

        // 检查认证ID和秘钥是否匹配
        Connection conn = DbUtil.getConnection();
        PreparedStatement pstmt1 = null;
        PreparedStatement pstmt2 = null;
        ResultSet rs1 = null;
        ResultSet rs2 = null;
        // 认证ID
        String sql1 = "select conf_value from t_conf where conf_key='auth_id'";
        // 认证秘钥
        String sql2 = "select conf_value from t_conf where conf_key='auth_key'";
        try {
            pstmt1 = conn.prepareStatement(sql1);
            rs1 = pstmt1.executeQuery();
            rs1.next();
            String realAuthId = rs1.getString("conf_value");

            pstmt2 = conn.prepareStatement(sql2);
            rs2 = pstmt2.executeQuery();
            rs2.next();
            String realAuthKey = rs2.getString("conf_value");

            if (authId.equals(realAuthId) && EncUtil.sha256Hex(authKey).equals(realAuthKey)) {
                // 认证成功
                return true;
            } else {
                // 认证失败
                return false;
            }
        } catch (SQLException e) {
            log.error("数据库操作异常: ", e);
            return false;
        } finally {
            DbUtil.closeResource(pstmt1, rs1);
            DbUtil.closeResource(pstmt2, rs2);
            DbUtil.closeConnection();
        }
    }

    /**
     * 修改认证信息中的密码
     *
     * @param password 新密码
     */
    public void changePassword(String password) {
        Connection conn = DbUtil.getConnection();
        String sql = "update t_conf set conf_value=? where conf_key='auth_key'";
        PreparedStatement pstmt = null;
        try {
            pstmt = conn.prepareStatement(sql);
            pstmt.setString(1, EncUtil.sha256Hex(password));
            pstmt.execute();
        } catch (SQLException e) {
            log.error("数据库操作异常: ", e);
        } finally {
            DbUtil.closeResource(pstmt);
            DbUtil.closeConnection();
        }
    }
}
