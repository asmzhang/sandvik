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
 * Declares the ARM64 native methods of the M3 test library.
 *
 * <p>The bodies live in unidbg_sandvik_eval/native/sandvik_bridge.c and are
 * executed by unidbg (an ARM64 emulator); every Java method they call back into
 * is executed by the embedded sandvik VM. The declarations exist so the test
 * drives a class that really declares native methods, exactly like an app would.
 */
public class NativeBridge {
	public static native String bridgeHello();

	public static native int bridgeAdd(int a, int b);
}
