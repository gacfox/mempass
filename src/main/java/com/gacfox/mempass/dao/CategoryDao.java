package com.gacfox.mempass.dao;

import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.util.ArrayList;
import java.util.List;

import com.gacfox.mempass.model.domain.Category;
import com.gacfox.mempass.util.DbUtil;
import lombok.extern.slf4j.Slf4j;

/**
 * 目录操作
 *
 * @author gacfox
 */
@Slf4j
public class CategoryDao {

    private static CategoryDao self = null;

    /**
     * 单例模式
     *
     * @return 单例自身对象
     */
    public static CategoryDao getInstance() {
        if (self == null) {
            self = new CategoryDao();
        }
        return self;
    }

    /**
     * 添加分类
     *
     * @param category 分类对象，ID字段会被忽略
     */
    public void addCategory(Category category) {
        String sql = "insert into t_category (category_name) values (?)";
        Connection conn = DbUtil.getConnection();
        PreparedStatement pstmt = null;
        try {
            pstmt = conn.prepareStatement(sql);
            pstmt.setString(1, category.getCategoryName());
            pstmt.execute();
        } catch (SQLException e) {
            log.error("数据库操作异常: ", e);
        } finally {
            DbUtil.closeResource(pstmt);
        }
    }

    /**
     * 更新分类
     *
     * @param category 分类对象，依据ID进行更新
     */
    public void updateCategory(Category category) {
        String sql = "update t_category set category_name=? where category_id=?";
        Connection conn = DbUtil.getConnection();
        PreparedStatement pstmt = null;
        try {
            pstmt = conn.prepareStatement(sql);
            pstmt.setString(1, category.getCategoryName());
            pstmt.setLong(2, category.getCateoryId());
            pstmt.execute();
        } catch (SQLException e) {
            log.error("数据库操作异常: ", e);
        } finally {
            DbUtil.closeResource(pstmt);
        }
    }

    /**
     * 根据分类ID查询分类
     *
     * @param categoryId 分类ID
     * @return 结果对象，不存在返回null
     */
    public Category queryCategoryById(Long categoryId) {
        Category result = null;
        Connection conn = DbUtil.getConnection();
        PreparedStatement pstmt = null;
        ResultSet rs = null;
        String sql = "select category_id, category_name from t_category where category_id=?";
        try {
            pstmt = conn.prepareStatement(sql);
            pstmt.setLong(1, categoryId);
            rs = pstmt.executeQuery();
            if (rs.next()) {
                result = Category.builder().cateoryId(rs.getLong("category_id"))
                        .categoryName(rs.getString("category_name")).build();
            }
        } catch (SQLException e) {
            log.error("数据库操作异常: ", e);
        } finally {
            DbUtil.closeResource(pstmt, rs);
        }
        return result;
    }

    /**
     * 查询所有的分类，关联查询所有的账户信息
     *
     * @return 包含查询结果的列表，无结果返回空列表
     */
    public List<Category> queryAllCategories() {
        List<Category> resultList = new ArrayList<>();
        Connection conn = DbUtil.getConnection();
        String sql = "select category_id, category_name from t_category";
        PreparedStatement pstmt = null;
        ResultSet rs = null;
        try {
            pstmt = conn.prepareStatement(sql);
            rs = pstmt.executeQuery();
            while (rs.next()) {
                Category category = Category.builder().cateoryId(rs.getLong("category_id"))
                        .categoryName(rs.getString("category_name")).build();
                resultList.add(category);
            }
        } catch (SQLException e) {
            log.error("数据库操作异常: ", e);
        } finally {
            DbUtil.closeResource(pstmt, rs);
        }
        return resultList;
    }

    /**
     * 根据账户ID查询分类对象
     *
     * @param accountId 账户ID
     * @return 分类对象，不存在返回null
     */
    public Category queryCategoryByAccountId(Long accountId) {
        Category category = null;
        Connection conn = DbUtil.getConnection();
        String sql = "select t_category.category_id, t_category.category_name from t_category inner join t_account on t_category.category_id=t_account.category_id where t_account.account_id=?";
        PreparedStatement pstmt = null;
        ResultSet rs = null;
        try {
            pstmt = conn.prepareStatement(sql);
            pstmt.setLong(1, accountId);
            rs = pstmt.executeQuery();
            if (rs.next()) {
                category = Category.builder().cateoryId(rs.getLong("category_id"))
                        .categoryName(rs.getString("category_name")).build();
            }
        } catch (SQLException e) {
            log.error("数据库操作异常: ", e);
        } finally {
            DbUtil.closeResource(pstmt, rs);
        }
        return category;
    }

    /**
     * 根据分类ID删除分类，账户信息不会级联删除，会被移动至默认分类下
     *
     * @param categoryId 分类ID
     */
    public void deleteCategoryById(Long categoryId) {
        Connection conn = DbUtil.getConnection();

        // 移动账户到默认分类
        String sql1 = "update t_account set category_id=? where category_id=?";
        PreparedStatement pstmt1 = null;
        try {
            pstmt1 = conn.prepareStatement(sql1);
            pstmt1.setLong(1, 1L);
            pstmt1.setLong(2, categoryId);
            pstmt1.execute();
        } catch (SQLException e) {
            log.error("数据库操作异常: ", e);
        } finally {
            DbUtil.closeResource(pstmt1);
        }
        // 删除分类
        String sql2 = "delete from t_category where category_id=?";
        PreparedStatement pstmt2 = null;
        try {
            pstmt2 = conn.prepareStatement(sql2);
            pstmt2.setLong(1, categoryId);
            pstmt2.execute();
        } catch (SQLException e) {
            log.error("数据库操作异常: ", e);
        } finally {
            DbUtil.closeResource(pstmt2);
        }
    }
}
