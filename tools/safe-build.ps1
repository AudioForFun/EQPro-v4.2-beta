param(
    [switch]$SkipSourceSync,
    [switch]$SkipInstaller,
    [switch]$AllowDirty,
    [string]$IsccPath = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Write-Step([string]$message)
{
    Write-Host ""
    Write-Host "==> $message"
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$srcRoot = Join-Path $repoRoot "src"
$sourceRoot = Join-Path $repoRoot "Source"
$buildRoot = Join-Path $repoRoot "build"
$issPath = Join-Path $repoRoot "installer\\EQPro.iss"
$installerOut = Join-Path $repoRoot "installer\\Output\\EQPro_Setup.exe"

if (-not (Test-Path $srcRoot))
{
    throw "Missing src/ folder at $srcRoot"
}

Write-Step "Pre-flight git status"
if (-not $AllowDirty)
{
    $status = git status --porcelain
    if ($status)
    {
        Write-Host $status
        throw "Working tree is dirty. Commit or stash changes, or rerun with -AllowDirty."
    }
}

if (-not $SkipSourceSync)
{
    Write-Step "Sync src/ -> Source/"
    if (-not (Test-Path $sourceRoot))
    {
        New-Item -ItemType Directory -Path $sourceRoot | Out-Null
    }

    $roots = @("dsp", "ui", "util")
    foreach ($dir in $roots)
    {
        $srcDir = Join-Path $srcRoot $dir
        $dstDir = Join-Path $sourceRoot $dir
        if (Test-Path $srcDir)
        {
            New-Item -ItemType Directory -Path $dstDir -Force | Out-Null
            & robocopy $srcDir $dstDir /E /NFL /NDL /NJH /NJS | Out-Null
        }
    }

    $rootFiles = @("PluginEditor.cpp", "PluginEditor.h", "PluginProcessor.cpp", "PluginProcessor.h")
    foreach ($file in $rootFiles)
    {
        Copy-Item (Join-Path $srcRoot $file) (Join-Path $sourceRoot $file) -Force
    }
}

Write-Step "Build Release"
if (-not (Test-Path $buildRoot))
{
    throw "Missing build/ folder. Configure with CMake first."
}
cmake --build $buildRoot --config Release

$vst3Path = Join-Path $buildRoot "EQPro_artefacts\\Release\\VST3\\EQ Pro.vst3"
$standalonePath = Join-Path $buildRoot "EQPro_artefacts\\Release\\Standalone\\EQ Pro.exe"
if (-not (Test-Path $vst3Path))
{
    throw "Missing VST3 output: $vst3Path"
}
if (-not (Test-Path $standalonePath))
{
    throw "Missing Standalone output: $standalonePath"
}

if (-not $SkipInstaller)
{
    Write-Step "Build installer"
    if ([string]::IsNullOrWhiteSpace($IsccPath))
    {
        $cmd = Get-Command ISCC.exe -ErrorAction SilentlyContinue
        if ($cmd -ne $null)
        {
            $IsccPath = $cmd.Source
        }
    }

    if ([string]::IsNullOrWhiteSpace($IsccPath) -or -not (Test-Path $IsccPath))
    {
        throw "ISCC.exe not found. Pass -IsccPath or add Inno Setup to PATH."
    }

    & $IsccPath $issPath

    if (-not (Test-Path $installerOut))
    {
        throw "Installer not found: $installerOut"
    }
}

Write-Step "Done"
Write-Host "VST3: $vst3Path"
Write-Host "Standalone: $standalonePath"
if (-not $SkipInstaller)
{
    Write-Host "Installer: $installerOut"
}
