$ErrorActionPreference = 'Stop'

$componentRoot = Split-Path -Parent $PSScriptRoot
$projectRoot = Split-Path -Parent (Split-Path -Parent $componentRoot)
$outputDirectory = Join-Path $projectRoot 'build\host-tests'
$outputFile = Join-Path $outputDirectory 'serial_gateway_tests.exe'

New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null

$gccArguments = @(
    '-std=c11'
    '-Wall'
    '-Wextra'
    '-Werror'
    (Join-Path $componentRoot 'test\test_serial_gateway.c')
    (Join-Path $componentRoot 'src\serial_gateway.c')
    (Join-Path $projectRoot 'components\frame_codec\src\frame_codec.c')
    '-I'
    (Join-Path $componentRoot 'test\include')
    '-I'
    (Join-Path $componentRoot 'include')
    '-I'
    (Join-Path $projectRoot 'components\frame_codec\include')
    '-o'
    $outputFile
)

& 'C:\cygwin\bin\gcc.exe' @gccArguments
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& $outputFile
