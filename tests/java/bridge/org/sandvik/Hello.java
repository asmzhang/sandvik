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
 * Reached *from ARM64 native code* through the JNI bridge (M3): the test library
 * unidbg_sandvik_eval/native/sandvik_bridge.c calls FindClass/GetStaticMethodID
 * and expects sandvik to execute these methods.
 *
 * <p>Unlike {@code BridgeHello} (default package, called directly through the C
 * API), this class sits in a package on purpose: it exercises the slashed name
 * path of {@code FindClass("org/sandvik/Hello")}.
 */
public class Hello {
	public static String hello(String name) {
		return "hello, " + name;
	}

	public static int add(int a, int b) {
		return a + b;
	}

	/** Identity probe across the native boundary: the argument has to come back. */
	public static String echo(String value) {
		return value;
	}
}
