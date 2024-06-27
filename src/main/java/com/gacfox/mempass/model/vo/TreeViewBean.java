package com.gacfox.mempass.model.vo;

import lombok.Builder;
import lombok.Data;

/**
 * 加载到TreeView中显示信息的Bean
 * 
 * @author gacfox
 */
@Data
@Builder
public class TreeViewBean {

	public static final int ROOT = 0;
	public static final int CATEGORY = 1;
	public static final int ACCOUNT = 2;

	private Long id;
	private String value;
	private int nodeType;

	private Object obj;
}
