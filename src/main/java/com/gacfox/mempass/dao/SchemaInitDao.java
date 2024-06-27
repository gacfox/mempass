package com.gacfox.mempass.dao;

import java.io.BufferedReader;
import java.io.File;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.SQLException;
import java.sql.Statement;
import java.util.ArrayList;
import java.util.List;

import com.gacfox.mempass.util.Config;
import com.gacfox.mempass.util.DbUtil;
import lombok.extern.slf4j.Slf4j;

/**
 * 数据库初始化
 *
 * @author gacfox
 */
@Slf4j
public class SchemaInitDao {

    private static SchemaInitDao self = null;

    /**
     * 单例模式
     *
     * @return 单例自身对象
     */
    public static SchemaInitDao getInstance() {
        if (self == null) {
            self = new SchemaInitDao();
        }
        return self;
    }

    /**
     * 检查工作路径是否已经初始化
     *
     * @return 已初始化返回true
     */
    public boolean initCheck() {
        String path = Config.WORK_DIR + "/data";
        File file = new File(path);
        return file.exists();
    }

    /**
     * 当数据库不存在（第一次运行）时，运行此方法初始化数据库表和数据
     */
    public void initSchema() {
        // 获取连接
        Connection conn = DbUtil.getConnection();
        Statement stmt = null;
        // 加载SQL
        InputStream is = SchemaInitDao.class.getClassLoader().getResourceAsStream("conf/db.sql");
        InputStreamReader isr = new InputStreamReader(is);
        BufferedReader br = new BufferedReader(isr);
        List<String> sqls = new ArrayList<>();
        StringBuilder sb = new StringBuilder();
        String line;
        try {
            while ((line = br.readLine()) != null) {
                sb.append(line);
                if (line.endsWith(";")) {
                    sqls.add(sb.toString());
                    sb = new StringBuilder();
                }
            }
            // 插入数据库
            conn.setAutoCommit(false);
            stmt = conn.createStatement();
            for (String sql : sqls) {
                stmt.addBatch(sql);
            }
            stmt.executeBatch();
            conn.commit();
        } catch (Exception e) {
            log.error("数据库操作异常: ", e);
        } finally {
            DbUtil.closeResource(stmt);
            DbUtil.closeConnection();
        }
    }

    /**
     * 在一切都初始化完成后调用，将t_conf的fresh字段设置为0
     */
    public void completeInitData() {
        Connection conn = DbUtil.getConnection();
        String sql = "update t_conf set conf_value='0' where conf_key='fresh'";
        PreparedStatement pstmt = null;
        try {
            pstmt = conn.prepareStatement(sql);
            pstmt.execute();
        } catch (SQLException e) {
            log.error("数据库操作异常: ", e);
        } finally {
            DbUtil.closeResource(pstmt);
            DbUtil.closeConnection();
        }
    }
}
