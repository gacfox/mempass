package com.gacfox.mempass.dao;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.text.SimpleDateFormat;
import java.util.List;

import com.gacfox.mempass.util.FileUtils;
import com.gacfox.mempass.model.domain.Account;
import com.gacfox.mempass.model.domain.Category;
import com.gacfox.mempass.util.Config;
import lombok.extern.slf4j.Slf4j;
import org.apache.poi.ss.util.CellRangeAddress;
import org.apache.poi.xssf.usermodel.XSSFCell;
import org.apache.poi.xssf.usermodel.XSSFRow;
import org.apache.poi.xssf.usermodel.XSSFSheet;
import org.apache.poi.xssf.usermodel.XSSFWorkbook;

/**
 * Excel表格导出
 *
 * @author gacfox
 */
@Slf4j
public class ExportDao {

    private final CategoryDao categoryDao = CategoryDao.getInstance();
    private final AccountDao accountDao = AccountDao.getInstance();

    private static ExportDao self = null;

    /**
     * 单例模式
     *
     * @return 单例自身对象
     */
    public static ExportDao getInstance() {
        if (self == null) {
            self = new ExportDao();
        }
        return self;
    }

    /**
     * 导出数据库
     *
     * @param file 目标文件
     */
    public void exportDb(File file) {
        String srcPath = Config.WORK_DIR + "/data/" + Config.AUTH_ID + "/mempass.mv.db";
        File srcFile = new File(srcPath);
        FileUtils.copyFile(srcFile, file);
    }

    /**
     * 导出到Excel
     *
     * @param file 目标文件
     */
    public void exportToExcel(File file) {
        XSSFWorkbook workbook = new XSSFWorkbook();
        XSSFSheet sheet = workbook.createSheet("password");
        List<Category> categories = categoryDao.queryAllCategories();
        int rowCnt = 0;
        int rowMergeStart = 0;
        for (Category category : categories) {
            List<Account> accounts = accountDao.queryAccountsByCategoryId(category.getCateoryId());
            for (Account account : accounts) {
                SimpleDateFormat sdf = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");
                XSSFRow row = sheet.createRow(rowCnt);
                XSSFCell cell0 = row.createCell(0);
                cell0.setCellValue(category.getCategoryName());
                XSSFCell cell1 = row.createCell(1);
                cell1.setCellValue(account.getItemName());
                XSSFCell cell2 = row.createCell(2);
                cell2.setCellValue(account.getUsername());
                XSSFCell cell3 = row.createCell(3);
                cell3.setCellValue(account.getPassword());
                XSSFCell cell4 = row.createCell(4);
                cell4.setCellValue(account.getDescription());
                XSSFCell cell5 = row.createCell(5);
                cell5.setCellValue(account.getNote());
                XSSFCell cell6 = row.createCell(6);
                cell6.setCellValue(sdf.format(account.getCreateTime()));
                XSSFCell cell7 = row.createCell(7);
                cell7.setCellValue(sdf.format(account.getLastModifiedTime()));
                XSSFCell cell8 = row.createCell(8);
                String availableStatus = "可用";
                switch (account.getAvailableStatus()) {
                    case 1:
                        availableStatus = "正常";
                        break;
                    case 2:
                        availableStatus = "不可用";
                        break;
                    case 3:
                        availableStatus = "废弃";
                        break;
                    case 4:
                        availableStatus = "已注销";
                        break;
                    default:
                        break;
                }
                cell8.setCellValue(availableStatus);
                rowCnt++;
            }
            int lenAccounts = accounts.size();
            if (lenAccounts > 1) {
                sheet.addMergedRegion(new CellRangeAddress(rowMergeStart, rowMergeStart + lenAccounts - 1, 0, 0));
            }
            rowMergeStart += lenAccounts;
        }
        FileOutputStream fos;
        try {
            fos = new FileOutputStream(file);
            workbook.write(fos);
            fos.close();
            workbook.close();
        } catch (IOException e) {
            log.error("数据库操作异常: ", e);
        }
    }
}
