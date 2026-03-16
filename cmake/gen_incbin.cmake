# Called at build time by add_custom_command.
# Variables: DEX_PATH (input), OUT (output .S file)
file(TO_CMAKE_PATH "${DEX_PATH}" DEX_PATH_FWD)

file(WRITE "${OUT}" "/* Auto-generated — do not edit */
    .section .rodata
    .global _binary_sanddirt_dex_jar_start
_binary_sanddirt_dex_jar_start:
    .incbin \"${DEX_PATH_FWD}\"
    .global _binary_sanddirt_dex_jar_end
_binary_sanddirt_dex_jar_end:
    .set _binary_sanddirt_dex_jar_size, (_binary_sanddirt_dex_jar_end - _binary_sanddirt_dex_jar_start)
    .global _binary_sanddirt_dex_jar_size
")
