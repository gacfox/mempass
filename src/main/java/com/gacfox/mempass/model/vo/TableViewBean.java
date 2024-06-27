package com.gacfox.mempass.model.vo;

import lombok.Builder;
import lombok.Data;

/**
 * 表格数据
 *
 * @author gacfox
 */
@Data
@Builder
public class TableViewBean {
    private String key;
    private String value;
    /**
     * 后加的扩展字段，用于TableView的密码Shadow显示功能
     */
    private String ext1;
}
