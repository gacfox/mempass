package com.gacfox.mempass.util;

/**
 * 字符串工具类
 * 
 * @author gacfox
 */
public class StringUtils {
	/**
	 * 判断源字符串是否包含指定目标字符串
	 * 
	 * @param srcStr  源字符串
	 * @param pattern 匹配目标
	 * @return 返回匹配结果
	 */
	public static boolean containsIgnoreCase(String srcStr, String pattern) {
		String srcStrLc = srcStr.toLowerCase();
		String patternLc = pattern.toLowerCase();
		return srcStrLc.contains(patternLc);
	}

	/**
	 * 判断字符串变量是否是空串或null
	 * 
	 * @param str 字符串
	 * @return 判断结果，空返回true
	 */
	public static boolean isEmpty(String str) {
        return str == null || str.isEmpty();
	}

	/**
	 * 将字符串转为密码圆点
	 * 
	 * @param srcStr 源字符串
	 * @return 相同长度的密码圆点字符串
	 */
	public static String toPasswordShadow(String srcStr) {
        return "●".repeat(srcStr.length());
	}
}
