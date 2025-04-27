# Compile the program using GCC
New-Item -Path ".\install\bootstrap" -ItemType Directory -Force

& gcc -O2 -I. `
    .\vendor\inih\ini.c `
    .\vendor\cwalk\cwalk.c `
    .\libcc\cc_strings.c `
    .\libcc\cc_allocator.c `
    .\libcc\cc_trie_map.c `
    .\libcc\cc_threadpool.c `
    .\libcc\cc_files.c `
    .\src\str_list.c `
    .\src\build_opts.c `
    .\src\cmd_build.c `
    .\src\cmd_clean.c `
    .\src\main.c -o .\install\bootstrap\cc.exe

# Add the install path to the PATH environment variable for the current session
$env:PATH += ";$(Get-Location)\install\bootstrap"

Write-Host "Program compiled and PATH updated for this session."
Write-Host "Command 'cc' is now available."
