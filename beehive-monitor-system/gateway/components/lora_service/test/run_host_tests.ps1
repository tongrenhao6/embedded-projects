$ErrorActionPreference = 'Stop'

$componentRoot = Split-Path -Parent $PSScriptRoot
$projectRoot = Split-Path -Parent (Split-Path -Parent $componentRoot)
$outputDirectory = Join-Path $projectRoot 'build\host-tests'
$outputFile = Join-Path $outputDirectory 'lora_stream_parser_tests.exe'

New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null

& 'C:\cygwin\bin\gcc.exe' `
    '-std=c11' '-Wall' '-Wextra' '-Werror' `
    (Join-Path $componentRoot 'test\test_lora_stream_parser.c') `
    (Join-Path $componentRoot 'src\lora_stream_parser.c') `
    (Join-Path $projectRoot 'components\frame_codec\src\frame_codec.c') `
    '-I' (Join-Path $componentRoot 'include') `
    '-I' (Join-Path $projectRoot 'components\frame_codec\include') `
    '-o' $outputFile

if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& $outputFile
