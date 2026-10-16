/*
 * This file is part of Sandvik project.
 * Copyright (C) 2025 Christophe Duvernois
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

package org.sandvik;

/**
 * M1 acceptance test of the sandvik-as-a-library milestone: a plain host JVM
 * loads libsandvik, feeds it the Android runtime jars and a dex file, then asks
 * it to execute a static method of that dex and returns the value to Java.
 *
 * Usage:
 *   java -cp build/bridge-classes -Dsandvik.library=<path to libsandvik.dll> \
 *        org.sandvik.BridgeTest <sandvik repo root>
 */
public final class BridgeTest {
	private static int failures = 0;

	public static void main(String[] args) {
		final String repo = args.length > 0 ? args[0] : ".";
		Native.setGlobalLogLevel(0);
		long handle = Native.create();
		check("sv_vm_create returns a handle", handle != 0);
		Native.setLogLevel(handle, 0);

		check("load core-oj.dex.jar", Native.loadRtFile(handle, repo + "/android/12.0.0/bin/core-oj.dex.jar") == 0);
		check("load core-libart.dex.jar", Native.loadRtFile(handle, repo + "/android/12.0.0/bin/core-libart.dex.jar") == 0);
		check("load icu-stubs.dex.jar", Native.loadRtFile(handle, repo + "/android/12.0.0/bin/icu-stubs.dex.jar") == 0);
		check("load tests/java/bridge/classes.dex", Native.loadDex(handle, repo + "/tests/java/bridge/classes.dex") == 0);

		final Object[] hello = Native.callStatic(handle, "BridgeHello", "hello", "(Ljava/lang/String;)Ljava/lang/String;",
		                                         new String[] {"s:world"});
		System.out.println("[1] BridgeHello.hello(\"world\") -> kind=" + kind(hello) + " value=" + text(hello));
		check("hello returns java.lang.String", kind(hello) != null && kind(hello) == Native.KIND_STRING);
		check("hello returns \"hello, world\"", "hello, world".equals(text(hello)));

		final Object[] add = Native.callStatic(handle, "BridgeHello", "add", "(II)I", new String[] {"i:40", "i:2"});
		System.out.println("[2] BridgeHello.add(40, 2)      -> kind=" + kind(add) + " value=" + number(add));
		check("add returns int", kind(add) != null && kind(add) == Native.KIND_INT);
		check("add returns 42", Long.valueOf(42L).equals(number(add)));

		Native.destroy(handle);
		System.out.println(failures == 0 ? "[ok] M1 acceptance passed" : "[ko] M1 acceptance failed: " + failures + " check(s)");
		System.exit(failures == 0 ? 0 : 1);
	}

	private static Integer kind(Object[] result) {
		return result != null && result.length > 0 ? (Integer) result[0] : null;
	}

	private static String text(Object[] result) {
		return result != null && result.length > 1 ? (String) result[1] : null;
	}

	private static Long number(Object[] result) {
		return result != null && result.length > 2 ? (Long) result[2] : null;
	}

	private static void check(String what, boolean ok) {
		if (ok) {
			System.out.println("  [ok] " + what);
		} else {
			failures++;
			System.out.println("  [ko] " + what);
		}
	}
}
