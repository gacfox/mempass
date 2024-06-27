package com.gacfox.mempass.dao;

import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Timestamp;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;

import com.gacfox.mempass.model.domain.Account;
import com.gacfox.mempass.util.Config;
import com.gacfox.mempass.util.DbUtil;
import com.gacfox.mempass.util.EncUtil;
import lombok.extern.slf4j.Slf4j;

/**
 * 账户操作
 *
 * @author gacfox
 */
@Slf4j
public class AccountDao {

    private static AccountDao self = null;

    /**
     * 单例模式
     *
     * @return 单例自身对象
     */
    public static AccountDao getInstance() {
        if (self == null) {
            self = new AccountDao();
        }
        return self;
    }

    /**
     * 根据账户信息ID查询账户信息
     *
     * @param accountId 账户信息ID
     * @return 账户信息
     */
    public Account queryAccountById(Long accountId) {
        Account account = null;
        Connection conn = DbUtil.getConnection();
        PreparedStatement pstmt = null;
        ResultSet rs = null;
        String sql = "select account_id,item_name,user_name,password,description,note,create_time,last_modified_time,available_status,category_id from t_account where account_id=?";
        try {
            pstmt = conn.prepareStatement(sql);
            pstmt.setLong(1, accountId);
            rs = pstmt.executeQuery();
            if (rs.next()) {
                account = Account.builder().accountId(rs.getLong("account_id")).itemName(rs.getString("item_name"))
                        .username(rs.getString("user_name"))
                        .password(EncUtil.pbeDecryptStr(rs.getString("password"), Config.AUTH_KEY))
                        .description(rs.getString("description")).note(rs.getString("note"))
                        .createTime(new Date(rs.getTimestamp("create_time").getTime()))
                        .lastModifiedTime(new Date(rs.getTimestamp("last_modified_time").getTime()))
                        .availableStatus(rs.getInt("available_status")).build();
            }
        } catch (SQLException e) {
            log.error("数据库操作异常: ", e);
        } finally {
            DbUtil.closeResource(pstmt, rs);
        }
        return account;
    }

    /**
     * 查询全部账户信息
     *
     * @return 全部账户信息列表
     */
    public List<Account> queryAllAccounts() {
        List<Account> resultList = new ArrayList<>();

        Connection conn = DbUtil.getConnection();
        PreparedStatement pstmt = null;
        ResultSet rs = null;
        String sql = "select account_id,item_name,user_name,password,description,note,create_time,last_modified_time,available_status,category_id from t_account";
        try {
            pstmt = conn.prepareStatement(sql);
            rs = pstmt.executeQuery();
            while (rs.next()) {
                Account account = Account.builder().accountId(rs.getLong("account_id"))
                        .itemName(rs.getString("item_name")).username(rs.getString("user_name"))
                        .password(EncUtil.pbeDecryptStr(rs.getString("password"), Config.AUTH_KEY))
                        .description(rs.getString("description")).note(rs.getString("note"))
                        .createTime(new Date(rs.getTimestamp("create_time").getTime()))
                        .lastModifiedTime(new Date(rs.getTimestamp("last_modified_time").getTime()))
                        .availableStatus(rs.getInt("available_status")).build();
                resultList.add(account);
            }
        } catch (SQLException e) {
            log.error("数据库操作异常: ", e);
        } finally {
            DbUtil.closeResource(pstmt, rs);
        }
        return resultList;
    }

    /**
     * 根据分类ID关联查询账户信息
     *
     * @param categoryId 分类ID
     * @return 包含查询结果的列表，无结果返回空列表
     */
    public List<Account> queryAccountsByCategoryId(Long categoryId) {
        List<Account> resultList = new ArrayList<>();

        // 查询该分类下的账户
        Connection conn = DbUtil.getConnection();
        PreparedStatement pstmt = null;
        ResultSet rs = null;
        String sql = "select account_id,item_name,user_name,password,description,note,create_time,last_modified_time,available_status,category_id from t_account where category_id=?";
        try {
            pstmt = conn.prepareStatement(sql);
            pstmt.setLong(1, categoryId);
            rs = pstmt.executeQuery();
            while (rs.next()) {
                Account account = Account.builder().accountId(rs.getLong("account_id"))
                        .itemName(rs.getString("item_name")).username(rs.getString("user_name"))
                        .password(EncUtil.pbeDecryptStr(rs.getString("password"), Config.AUTH_KEY))
                        .description(rs.getString("description")).note(rs.getString("note"))
                        .createTime(new Date(rs.getTimestamp("create_time").getTime()))
                        .lastModifiedTime(new Date(rs.getTimestamp("last_modified_time").getTime()))
                        .availableStatus(rs.getInt("available_status")).build();
                resultList.add(account);
            }
        } catch (SQLException e) {
            log.error("数据库操作异常: ", e);
        } finally {
            DbUtil.closeResource(pstmt, rs);
        }
        return resultList;
    }

    /**
     * 插入账户信息，ID字段会被忽略
     *
     * @param account 账户信息
     */
    public void addAccount(Account account, Long categoryId) {
        Connection conn = DbUtil.getConnection();
        PreparedStatement pstmt = null;
        String sql = "insert into t_account(item_name,user_name,password,description,note,create_time,last_modified_time,available_status,category_id) values (?,?,?,?,?,?,?,?,?)";
        try {
            pstmt = conn.prepareStatement(sql);
            pstmt.setString(1, account.getItemName());
            pstmt.setString(2, account.getUsername());
            pstmt.setString(3, EncUtil.pbeEncryptStr(account.getPassword(), Config.AUTH_KEY));
            pstmt.setString(4, account.getDescription());
            pstmt.setString(5, account.getNote());
            pstmt.setTimestamp(6, new Timestamp(account.getCreateTime().getTime()));
            pstmt.setTimestamp(7, new Timestamp(account.getLastModifiedTime().getTime()));
            pstmt.setInt(8, account.getAvailableStatus());
            pstmt.setLong(9, categoryId);
            pstmt.execute();
        } catch (SQLException e) {
            log.error("数据库操作异常: ", e);
        } finally {
            DbUtil.closeResource(pstmt);
        }
    }

    /**
     * 更新账户信息，依据ID字段
     *
     * @param account    账户信息对象
     * @param categoryId 分类ID
     */
    public void updateAccount(Account account, Long categoryId) {
        Connection conn = DbUtil.getConnection();
        String sql = "update t_account set item_name=?,user_name=?,password=?,description=?,note=?,create_time=?,last_modified_time=?,available_status=?,category_id=? where account_id=?";
        PreparedStatement pstmt = null;
        try {
            pstmt = conn.prepareStatement(sql);
            pstmt.setString(1, account.getItemName());
            pstmt.setString(2, account.getUsername());
            pstmt.setString(3, EncUtil.pbeEncryptStr(account.getPassword(), Config.AUTH_KEY));
            pstmt.setString(4, account.getDescription());
            pstmt.setString(5, account.getNote());
            pstmt.setTimestamp(6, new Timestamp(account.getCreateTime().getTime()));
            pstmt.setTimestamp(7, new Timestamp(account.getLastModifiedTime().getTime()));
            pstmt.setInt(8, account.getAvailableStatus());
            pstmt.setLong(9, categoryId);
            pstmt.setLong(10, account.getAccountId());
            pstmt.execute();
        } catch (SQLException e) {
            log.error("数据库操作异常: ", e);
        } finally {
            DbUtil.closeResource(pstmt);
        }
    }

    /**
     * 根据账户ID彻底删除账户信息
     *
     * @param accountId 账户ID
     */
    public void deleteAccountById(Long accountId) {
        Connection conn = DbUtil.getConnection();
        PreparedStatement pstmt = null;
        String sql = "delete from t_account where account_id=?";
        try {
            pstmt = conn.prepareStatement(sql);
            pstmt.setLong(1, accountId);
            pstmt.execute();
        } catch (SQLException e) {
            log.error("数据库操作异常: ", e);
        } finally {
            DbUtil.closeResource(pstmt);
        }
    }

    /**
     * 更新所有的加密秘钥
     *
     * @param oldPassword 旧密码
     * @param newPassword 新密码
     */
    public void updateEncKey(String oldPassword, String newPassword) {
        List<Account> accounts = queryAllAccounts();
        String sql = "update t_account set password=? where account_id=?";
        Connection conn = DbUtil.getConnection();
        PreparedStatement pstmt = null;
        try {
            conn.setAutoCommit(false);
            pstmt = conn.prepareStatement(sql);
            for (Account account : accounts) {
                String data = account.getPassword();
                String encNew = EncUtil.pbeEncryptStr(data, newPassword);
                pstmt.setString(1, encNew);
                pstmt.setLong(2, account.getAccountId());
                pstmt.addBatch();
            }
            pstmt.executeBatch();
            conn.commit();
        } catch (SQLException e) {
            log.error("数据库操作异常: ", e);
        } finally {
            DbUtil.closeResource(pstmt);
        }
    }
}
