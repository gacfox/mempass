package com.gacfox.mempass.model.domain;

import java.io.Serial;
import java.io.Serializable;
import java.util.Date;

import lombok.AllArgsConstructor;
import lombok.Builder;
import lombok.Data;
import lombok.NoArgsConstructor;

/**
 * 账户
 *
 * @author gacfox
 */
@Data
@Builder
@NoArgsConstructor
@AllArgsConstructor
public class Account implements Serializable {

    @Serial
    private static final long serialVersionUID = 1L;

    public static final int AVAILABLE = 1;
    public static final int UNAVAILABLE = 2;
    public static final int DEPRECATED = 3;
    public static final int CANCELLED = 4;

    private Long accountId;
    private String itemName;
    private String username;
    private String password;
    private String description;
    private String note;
    private Date createTime;
    private Date lastModifiedTime;
    private Integer availableStatus;
}
