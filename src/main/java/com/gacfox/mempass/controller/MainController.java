package com.gacfox.mempass.controller;

import java.awt.Desktop;
import java.io.File;
import java.net.URI;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.List;

import com.gacfox.mempass.util.DbUtil;
import com.gacfox.mempass.util.StringUtils;
import com.gacfox.mempass.SystemTrayInitializr;
import com.gacfox.mempass.WindowInitializr;
import com.gacfox.mempass.dao.AccountDao;
import com.gacfox.mempass.dao.CategoryDao;
import com.gacfox.mempass.dao.ExportDao;
import com.gacfox.mempass.model.domain.Account;
import com.gacfox.mempass.model.domain.Category;
import com.gacfox.mempass.model.vo.TableViewBean;
import com.gacfox.mempass.model.vo.TreeViewBean;

import javafx.application.Platform;
import javafx.beans.value.ChangeListener;
import javafx.collections.FXCollections;
import javafx.collections.ObservableList;
import javafx.fxml.FXML;
import javafx.scene.control.Alert;
import javafx.scene.control.ButtonType;
import javafx.scene.control.ContextMenu;
import javafx.scene.control.MenuItem;
import javafx.scene.control.TableColumn;
import javafx.scene.control.TableView;
import javafx.scene.control.TextField;
import javafx.scene.control.TreeCell;
import javafx.scene.control.TreeItem;
import javafx.scene.control.TreeView;
import javafx.scene.control.Alert.AlertType;
import javafx.scene.control.Button;
import javafx.scene.control.cell.TextFieldTableCell;
import javafx.stage.FileChooser;
import lombok.extern.slf4j.Slf4j;

/**
 * 主界面
 *
 * @author gacfox
 */
@Slf4j
public class MainController {

    @FXML
    private MenuItem miNewCategory;
    @FXML
    private MenuItem miNewAccount;
    @FXML
    private MenuItem miUpdateCategory;
    @FXML
    private MenuItem miUpdateAccount;
    @FXML
    private MenuItem miDeleteCategory;
    @FXML
    private MenuItem miDeleteAccount;

    @FXML
    private Button btnNewCategory;
    @FXML
    private Button btnNewAccount;
    @FXML
    private Button btnDeleteCategory;
    @FXML
    private Button btnDeleteAccount;

    @FXML
    private TreeView<TreeViewBean> tvCategory;
    @FXML
    private TableView<TableViewBean> tvAccountInfo;
    @FXML
    private TableColumn<TableViewBean, String> tcAccountInfoKey;
    @FXML
    private TableColumn<TableViewBean, String> tcAccountInfoValue;
    @FXML
    private TextField tfSearch;

    private static MainController self = null;

    private final CategoryDao categoryDao = CategoryDao.getInstance();
    private final AccountDao accountDao = AccountDao.getInstance();
    private final ExportDao exportDao = ExportDao.getInstance();

    private final WindowInitializr windowInitializr = WindowInitializr.getInstance();

    /**
     * 单例模式获取控制器引用
     *
     * @return MainController引用
     */
    public static MainController getInstance() {
        return MainController.self;
    }

    /**
     * 导出数据源
     */
    public void handleExportDataSourceButton() {
        FileChooser fileChooser = new FileChooser();
        FileChooser.ExtensionFilter extFilter = new FileChooser.ExtensionFilter("H2 DB files (*.db)", "*.db");
        fileChooser.getExtensionFilters().add(extFilter);
        fileChooser.setInitialFileName("mempass.mv.db");
        fileChooser.setTitle("保存数据库");
        File targetFile = fileChooser.showSaveDialog(windowInitializr.STAGE_MAIN);
        if (targetFile != null) {
            exportDao.exportDb(targetFile);
        }
    }

    /**
     * 导出Excel
     */
    public void handleExportExcelButton() {
        FileChooser fileChooser = new FileChooser();
        FileChooser.ExtensionFilter extFilter = new FileChooser.ExtensionFilter("Microsoft Excel files (*.xlsx)",
                "*.xlsx");
        fileChooser.getExtensionFilters().add(extFilter);
        fileChooser.setInitialFileName("password.xlsx");
        fileChooser.setTitle("导出数据为Excel");
        File targetFile = fileChooser.showSaveDialog(windowInitializr.STAGE_MAIN);
        if (targetFile != null) {
            exportDao.exportToExcel(targetFile);
        }
    }

    /**
     * 退出程序
     */
    public void handleExitProgramButton() {
        DbUtil.closeConnection();
        Platform.exit();
    }

    /**
     * 退出登录
     */
    public void handleExitLoginButton() {
        DbUtil.closeConnection();
        windowInitializr.STAGE_MAIN.hide();
        LoginController.getInstance().clearTfAuthKey();
        windowInitializr.STAGE_LOGIN.show();
        SystemTrayInitializr.getInstance().setCurrentStage(windowInitializr.STAGE_LOGIN);
    }

    /**
     * 改密码
     */
    public void handleChangePasswordButton() {
        try {
            windowInitializr.initStageChangePasswordDialog();
            windowInitializr.STAGE_CHANGE_PASSWORD_DIALOG.show();
        } catch (Exception e) {
            log.error("切换界面失败: ", e);
        }
    }

    /**
     * 创建分类
     */
    public void handleNewCategoryButton() {
        try {
            windowInitializr.initStageNewCategoryDialog();
            NewCategoryDialogController.getInstance().setUpsertFlag(0);
            NewCategoryDialogController.getInstance().setUpdateId(null);
            windowInitializr.STAGE_NEW_CATEGORY_DIALOG.show();
        } catch (Exception e) {
            log.error("切换界面失败: ", e);
        }
    }

    /**
     * 创建账户信息
     */
    public void handleNewAccountButton() {
        try {
            windowInitializr.initStageNewAccountDialog();
            NewAccountDialogController.getInstance().setUpsertFlag(0);
            NewAccountDialogController.getInstance().setUpdateId(null);
            windowInitializr.STAGE_NEW_ACCOUNT_DIALOG.show();
        } catch (Exception e) {
            log.error("切换界面失败: ", e);
        }
    }

    /**
     * 更新分类
     */
    public void handleUpdateCategoryButton() {
        TreeItem<TreeViewBean> selectedItem = tvCategory.getSelectionModel().getSelectedItem();
        if (selectedItem != null) {
            TreeViewBean selectedBean = selectedItem.getValue();
            if (selectedBean.getNodeType() == TreeViewBean.CATEGORY) {
                long toUpdateCategoryId = selectedBean.getId();
                try {
                    windowInitializr.initStageNewCategoryDialog();
                    NewCategoryDialogController.getInstance().setUpsertFlag(1);
                    NewCategoryDialogController.getInstance().setUpdateId(toUpdateCategoryId);
                    NewCategoryDialogController.getInstance().initUpdateData();
                    windowInitializr.STAGE_NEW_CATEGORY_DIALOG.show();
                } catch (Exception e) {
                    log.error("切换界面失败: ", e);
                }
            }
        }
    }

    /**
     * 更新账户信息
     */
    public void handleUpdateAccountButton() {
        TreeItem<TreeViewBean> selectedItem = tvCategory.getSelectionModel().getSelectedItem();
        if (selectedItem != null) {
            TreeViewBean selectedBean = selectedItem.getValue();
            if (selectedBean.getNodeType() == TreeViewBean.ACCOUNT) {
                long toUpdateAccountId = selectedBean.getId();
                try {
                    windowInitializr.initStageNewAccountDialog();
                    NewAccountDialogController.getInstance().setUpsertFlag(1);
                    NewAccountDialogController.getInstance().setUpdateId(toUpdateAccountId);
                    NewAccountDialogController.getInstance().initUpdateData();
                    windowInitializr.STAGE_NEW_ACCOUNT_DIALOG.show();
                } catch (Exception e) {
                    log.error("切换界面失败: ", e);
                }
            }
        }
    }

    /**
     * 删除分类
     */
    public void handleDeleteCategoryButton() {
        TreeItem<TreeViewBean> selectedItem = tvCategory.getSelectionModel().getSelectedItem();
        if (selectedItem != null) {
            TreeViewBean selectedBean = selectedItem.getValue();
            if (selectedBean.getNodeType() == TreeViewBean.CATEGORY) {
                if (selectedBean.getId() == 1L) {
                    Alert alert = new Alert(AlertType.WARNING);
                    alert.setTitle("警告");
                    alert.setContentText("不允许删除默认分类！");
                    alert.show();
                    return;
                } else {
                    long toDeleteCategoryId = selectedBean.getId();
                    Alert alert = new Alert(AlertType.CONFIRMATION);
                    alert.setTitle("警告");
                    alert.setContentText("你真的要删除该分类吗？级联账户信息会被移动至默认分类下。");
                    alert.showAndWait();
                    if (alert.getResult() == ButtonType.OK) {
                        categoryDao.deleteCategoryById(toDeleteCategoryId);
                        reloadTreeView();
                    }
                }
            }
        }
    }

    /**
     * 删除账户信息
     */
    public void handleDeleteAccountButton() {
        TreeItem<TreeViewBean> selectedItem = tvCategory.getSelectionModel().getSelectedItem();
        if (selectedItem != null) {
            TreeViewBean selectedBean = selectedItem.getValue();
            if (selectedBean.getNodeType() == TreeViewBean.ACCOUNT) {
                long toDeleteAccountId = selectedBean.getId();
                Alert alert = new Alert(AlertType.CONFIRMATION);
                alert.setTitle("警告");
                alert.setContentText("你真的要彻底删除该账户信息吗？");
                alert.showAndWait();
                if (alert.getResult() == ButtonType.OK) {
                    accountDao.deleteAccountById(toDeleteAccountId);
                    reloadTreeView();
                    clearTable();
                }
            }
        }
    }

    /**
     * 随机密码窗口
     */
    public void handleRandPasswordDialogButton() {
        try {
            windowInitializr.initStageRandomPasswordDialog();
            windowInitializr.STAGE_RANDOM_PASSWORD_DIALOG.show();
        } catch (Exception e) {
            log.error("切换界面失败: ", e);
        }
    }

    /**
     * 关于窗口
     */
    public void handleAboutDialogButton() {
        try {
            windowInitializr.initStageAboutDialog();
            windowInitializr.STAGE_ABOUT_DIALOG.show();
        } catch (Exception e) {
            log.error("切换界面失败: ", e);
        }
    }

    /**
     * 文档
     */
    public void handleShowDocumentButton() {
        if (Desktop.isDesktopSupported() && Desktop.getDesktop().isSupported(Desktop.Action.BROWSE)) {
            try {
                Desktop.getDesktop().browse(new URI("https://github.com/gacfox/mempass"));
            } catch (Exception e) {
                log.error("切换界面失败: ", e);
            }
        }
    }

    /**
     * 搜索功能
     */
    public void handleSearchButton() {
        String pattern = tfSearch.getText();
        reloadTreeView(pattern);
    }

    /**
     * 清空搜索条件并刷新
     */
    public void handleClearSearchButton() {
        tfSearch.setText("");
        reloadTreeView(null);
    }

    /**
     * 树形菜单定制cell
     */
    class CategoryTreeCell extends TreeCell<TreeViewBean> {
        @Override
        protected void updateItem(TreeViewBean item, boolean empty) {
            // 设置TreeCell文字内容
            super.updateItem(item, empty);
            setText(item == null ? "" : item.getValue());
            // 设置节点文字颜色，为了避免TreeCell实例自动复用引起bug，设置颜色前重置为黑色
            setStyle("-fx-text-fill: #000000;");
            if (item != null && item.getNodeType() == TreeViewBean.ACCOUNT) {
                Account account = (Account) item.getObj();
                switch (account.getAvailableStatus()) {
                    case Account.AVAILABLE:
                        setStyle("-fx-text-fill: #000000;");
                        break;
                    case Account.UNAVAILABLE:
                        setStyle("-fx-text-fill: #F0AD4E;");
                        break;
                    case Account.DEPRECATED:
                        setStyle("-fx-text-fill: #CBCBCB;");
                        break;
                    case Account.CANCELLED:
                        setStyle("-fx-text-fill: #FF0000;");
                        break;
                    default:
                        break;
                }
            }
            // 设置右键菜单
            ContextMenu contextMenu = new ContextMenu();
            if (item != null && item.getNodeType() == TreeViewBean.CATEGORY) {
                MenuItem mi1 = new MenuItem("新建类别");
                mi1.setOnAction(event -> handleNewCategoryButton());
                MenuItem mi2 = new MenuItem("修改类别");
                mi2.setOnAction(event -> handleUpdateCategoryButton());
                MenuItem mi3 = new MenuItem("删除类别");
                mi3.setOnAction(event -> handleDeleteCategoryButton());
                contextMenu.getItems().add(mi1);
                contextMenu.getItems().add(mi2);
                contextMenu.getItems().add(mi3);
            } else if (item != null && item.getNodeType() == TreeViewBean.ACCOUNT) {
                MenuItem mi1 = new MenuItem("新建账户");
                mi1.setOnAction(event -> handleNewAccountButton());
                MenuItem mi2 = new MenuItem("修改账户");
                mi2.setOnAction(event -> handleUpdateAccountButton());
                MenuItem mi3 = new MenuItem("删除账户");
                mi3.setOnAction(event -> handleDeleteAccountButton());
                contextMenu.getItems().add(mi1);
                contextMenu.getItems().add(mi2);
                contextMenu.getItems().add(mi3);
            }
            this.setContextMenu(contextMenu);
        }
    }

    @FXML
    public void initialize() {
        // 注册外部调用接口
        MainController.self = this;
        // 设置树形菜单CellFactory
        tvCategory.setCellFactory(param -> new CategoryTreeCell());
        // 设置树形菜单监听器
        tvCategory.getSelectionModel().selectedItemProperty()
                .addListener((ChangeListener<TreeItem<TreeViewBean>>) (observable, oldValue, newValue) -> {
                    if (newValue != null) {
                        TreeViewBean selectedBean = newValue.getValue();
                        if (selectedBean.getNodeType() == TreeViewBean.ACCOUNT) {
                            // 选择的是Account节点
                            loadTable((Account) selectedBean.getObj());
                            // 更新工具栏按钮和菜单按钮状态
                            miUpdateCategory.setDisable(true);
                            miUpdateAccount.setDisable(false);
                            miDeleteCategory.setDisable(true);
                            miDeleteAccount.setDisable(false);
                            btnDeleteCategory.setDisable(true);
                            btnDeleteAccount.setDisable(false);
                        } else if (selectedBean.getNodeType() == TreeViewBean.CATEGORY) {
                            // 选择的是Category节点
                            clearTable();
                            // 更新工具栏按钮和菜单按钮状态
                            miUpdateCategory.setDisable(false);
                            miUpdateAccount.setDisable(true);
                            miDeleteCategory.setDisable(false);
                            miDeleteAccount.setDisable(true);
                            btnDeleteCategory.setDisable(false);
                            btnDeleteAccount.setDisable(true);
                        } else {
                            // 选择的是ROOT节点
                            clearTable();
                            // 更新工具栏按钮和菜单按钮状态
                            miUpdateCategory.setDisable(false);
                            miUpdateAccount.setDisable(false);
                            miDeleteCategory.setDisable(false);
                            miDeleteAccount.setDisable(false);
                            btnDeleteCategory.setDisable(false);
                            btnDeleteAccount.setDisable(false);
                        }
                    }
                });
        // 刷新树形菜单
        reloadTreeView();
        // 设置表格数据存储和展示逻辑
        tcAccountInfoKey.setCellFactory(TextFieldTableCell.<TableViewBean>forTableColumn());
        tcAccountInfoValue.setCellFactory(TextFieldTableCell.<TableViewBean>forTableColumn());
        // 只有当密码行选中时才显示明文
        // 由于我想要的效果和TableView默认的行为不太一致，所以这里代码看起来比较奇怪
        // 具体我没细看，但我赌5毛JavaFX的表格渲染缓冲逻辑和数据绑定逻辑放一块有Bug
        tvAccountInfo.getSelectionModel().selectedItemProperty().addListener((obs, oldSelection, newSelection) -> {
            if (oldSelection != null && oldSelection.getExt1() != null) {
                oldSelection.setValue(StringUtils.toPasswordShadow(oldSelection.getExt1()));
                tvAccountInfo.refresh();
            }
            if (newSelection != null && newSelection.getExt1() != null) {
                newSelection.setValue(newSelection.getExt1());
                tvAccountInfo.refresh();
            }
        });
    }

    /**
     * 基于数据库重新加载左侧树形菜单
     */
    public void reloadTreeView() {
        reloadTreeView(null);
    }

    /**
     * 基于数据库重新加载左侧树形菜单，带搜索功能
     *
     * @param searchPattern 搜索匹配字符串
     */
    public void reloadTreeView(String searchPattern) {

        // 旧的树结构，用来遍历获取节点的expanded状态
        TreeItem<TreeViewBean> oldRootNode = tvCategory.getRoot();

        // 组装新的根节点
        TreeItem<TreeViewBean> rootNode = new TreeItem<>(
                TreeViewBean.builder().id(null).value("账号目录").nodeType(TreeViewBean.ROOT).obj(null).build());

        // 设置根节点expanded状态
        if (oldRootNode != null) {
            rootNode.setExpanded(oldRootNode.isExpanded());
        } else {
            rootNode.setExpanded(true);
        }

        // 分类组装
        List<Category> categoryList = categoryDao.queryAllCategories();
        for (Category category : categoryList) {
            TreeItem<TreeViewBean> categoryNode = new TreeItem<>(TreeViewBean.builder().id(category.getCateoryId())
                    .value(category.getCategoryName()).nodeType(TreeViewBean.CATEGORY).obj(category).build());

            // 设置分类expanded状态
            if (oldRootNode != null) {
                for (TreeItem<TreeViewBean> oldCategoryItemNode : oldRootNode.getChildren()) {
                    if (oldCategoryItemNode.getValue().getId().equals(category.getCateoryId())) {
                        categoryNode.setExpanded(oldCategoryItemNode.isExpanded());
                        break;
                    }
                }
            }

            rootNode.getChildren().add(categoryNode);

            // 类账户列表组装
            List<Account> accountList = accountDao.queryAccountsByCategoryId(category.getCateoryId());
            if (searchPattern != null && !searchPattern.isEmpty()) {
                // 在应用层根据匹配字符串过滤
                accountList.removeIf(nextAccount -> !StringUtils.containsIgnoreCase(nextAccount.getItemName(), searchPattern) &&
                        !StringUtils.containsIgnoreCase(nextAccount.getUsername(), searchPattern) &&
                        !StringUtils.containsIgnoreCase(nextAccount.getDescription(), searchPattern) &&
                        !StringUtils.containsIgnoreCase(nextAccount.getNote(), searchPattern));
            }
            for (Account account : accountList) {
                TreeItem<TreeViewBean> accountNode = new TreeItem<>(TreeViewBean.builder().id(account.getAccountId())
                        .value(account.getItemName()).nodeType(TreeViewBean.ACCOUNT).obj(account).build());
                categoryNode.getChildren().add(accountNode);
            }
        }
        tvCategory.setRoot(rootNode);
    }

    /**
     * 清空信息表格
     */
    public void clearTable() {
        ObservableList<TableViewBean> list = FXCollections.observableArrayList();
        tvAccountInfo.setItems(list);
    }

    /**
     * 基于数据库重新加载账号信息表格
     */
    public void reloadTable(Long accountId) {
        Account account = accountDao.queryAccountById(accountId);
        if (account != null) {
            loadTable(account);
        }
    }

    /**
     * 基于对象加载账号信息表格
     *
     * @param account 账户信息对象
     */
    public void loadTable(Account account) {
        // 清除选中状态
        tvAccountInfo.getSelectionModel().clearSelection();
        // 清除编辑状态
        // 貌似TableView没提供清除编辑状态的方法
        // 但是突然发现tableView.edit()参数的注释文档中写道，如下传参数会清除选中状态
        // 试了下居然好使，吃惊(⊙ˍ⊙)
        tvAccountInfo.edit(-1, null);
        // 加载数据
        TableViewBean tvb1 = TableViewBean.builder().key("账户项").value(account.getItemName()).build();
        TableViewBean tvb2 = TableViewBean.builder().key("账户名").value(account.getUsername()).build();
        String password = account.getPassword();
        String passwordDisplayShadow = StringUtils.toPasswordShadow(password);
        TableViewBean tvb3 = TableViewBean.builder().key("密码").value(passwordDisplayShadow).ext1(password).build();
        TableViewBean tvb4 = TableViewBean.builder().key("描述").value(account.getDescription()).build();
        TableViewBean tvb5 = TableViewBean.builder().key("备注").value(account.getNote()).build();
        Date createTime = account.getCreateTime();
        SimpleDateFormat sdf = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");
        String createTimeStr = sdf.format(createTime);
        TableViewBean tvb6 = TableViewBean.builder().key("创建时间").value(createTimeStr).build();
        Date lastModifiedTime = account.getLastModifiedTime();
        String lastModifiedTimeStr = sdf.format(lastModifiedTime);
        TableViewBean tvb7 = TableViewBean.builder().key("最后修改时间").value(lastModifiedTimeStr).build();
        int availableStatus = account.getAvailableStatus();
        String availableStatusStr = "正常";
        switch (availableStatus) {
            case Account.AVAILABLE:
                availableStatusStr = "正常";
                break;
            case Account.UNAVAILABLE:
                availableStatusStr = "不可用";
                break;
            case Account.DEPRECATED:
                availableStatusStr = "废弃";
                break;
            case Account.CANCELLED:
                availableStatusStr = "已注销";
                break;
            default:
                break;
        }
        TableViewBean tvb8 = TableViewBean.builder().key("可用状态").value(availableStatusStr).build();
        ObservableList<TableViewBean> list = FXCollections.observableArrayList();
        list.add(tvb1);
        list.add(tvb2);
        list.add(tvb3);
        list.add(tvb4);
        list.add(tvb5);
        list.add(tvb6);
        list.add(tvb7);
        list.add(tvb8);
        tvAccountInfo.setItems(list);
    }
}
