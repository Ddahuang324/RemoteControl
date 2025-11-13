# Build script for RemoteControl
param(
    [string]$Project = "RemoteControl.sln",
    [string]$Configuration = "Debug",
    [string]$Platform = "x64"
)

# Set VC environment variables directly
$env:VCInstallDir = "D:\Visual Studio\VC\"
$env:VCToolsVersion = "14.43.34808"
$env:VCToolsInstallDir = "D:\Visual Studio\VC\Tools\MSVC\14.43.34808\"
$env:VCTargetsPath = "D:\Visual Studio\MSBuild\Microsoft\VC\v160\"

# Also set for vcvars in case we need it
$vcvarsPath = "D:\Visual Studio\VC\Auxiliary\Build\vcvars64.bat"

# Run vcvars64.bat in a cmd shell and capture the environment
$env_output = cmd /c "$vcvarsPath && set" 2>&1
foreach ($line in $env_output) {
    if ($line -match '=') {
        $parts = $line -split '=', 2
        [Environment]::SetEnvironmentVariable($parts[0], $parts[1])
    }
}

# Run msbuild with explicit VC paths and disable auto response file
Write-Host "Building $Project..."
Write-Host "Using VCTargetsPath: $env:VCTargetsPath"
& msbuild $Project "/p:Configuration=$Configuration" "/p:Platform=$Platform" "/p:VCTargetsPath=$env:VCTargetsPath" "/p:VCInstallDir=$env:VCInstallDir" -noautoresponse
exit $LASTEXITCODE
