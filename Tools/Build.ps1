param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [ValidateSet('x64', 'Win32')][string]$Platform = 'x64',
    [switch]$Verify
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$solutionRoot = Join-Path $repoRoot 'PikachuVolleyBall'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio Installer/vswhere.exe is required.' }
$msbuild = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (-not $msbuild) { throw 'Install Visual Studio with Desktop development with C++ and a Windows SDK.' }

foreach ($name in @('Client', 'Server', 'Tests')) {
    $project = Join-Path $solutionRoot "$name/$name.vcxproj"
    & $msbuild $project /nologo /m /v:minimal "/p:Configuration=$Configuration" "/p:Platform=$Platform"
    if ($LASTEXITCODE -ne 0) { throw "$name build failed ($LASTEXITCODE)." }
}

if ($Verify) {
    $outputRoot = Join-Path $solutionRoot "bin/$Platform/$Configuration"
    & (Join-Path $outputRoot 'Server/Server.exe') --check
    if ($LASTEXITCODE -ne 0) { throw 'Server initialization check failed.' }
    & (Join-Path $outputRoot 'Tests/Tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Foundation checks failed.' }
    foreach ($resource in @('images/sprite_sheet.bmp', 'images/warning.bmp', 'sounds/bgm.wav', 'sounds/Stage2_Bgm.wav')) {
        if (-not (Test-Path -LiteralPath (Join-Path $outputRoot "Client/Resources/$resource"))) {
            throw "Missing client resource: $resource"
        }
    }
    Write-Host 'Verified: server initialization, protocol/TCP/input tests and copied client resources.'
}
