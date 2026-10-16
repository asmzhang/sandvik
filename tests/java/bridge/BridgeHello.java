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

/**
 * Class executed *by sandvik* (not by the host JVM), used by the bridge
 * acceptance test: tests/bridge/org/sandvik/BridgeTest.java calls into
 * libsandvik, which interprets this class out of classes.dex.
 */
public class BridgeHello {
	public static String hello(String name) {
		return "hello, " + name;
	}

	public static int add(int a, int b) {
		return a + b;
	}
}
