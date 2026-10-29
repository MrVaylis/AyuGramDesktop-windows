param(
    [Parameter(Mandatory = $true)]
    [string] $SourceRoot
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$SourceRoot = (Resolve-Path -LiteralPath $SourceRoot).Path
$metadata = Get-Content -LiteralPath "$SourceRoot\out\windows-metadata.json" -Raw | ConvertFrom-Json
$qtVersion = (Get-Content -LiteralPath "$SourceRoot\out\CMakeCache.txt" | Select-String '^qt_requested:STRING=(.+)$').Matches.Groups[1].Value
if ($qtVersion -notmatch '^6\.') { throw 'The release must use Qt 6.' }
$release = Join-Path $SourceRoot 'out\Release'
$binary = Join-Path $release 'AyuGram.exe'
$version = [Diagnostics.FileVersionInfo]::GetVersionInfo($binary)
if ($version.ProductName -ne 'AyuGram Desktop' -or $version.FileDescription -ne 'AyuGram Desktop') {
    throw 'Executable lost the AyuGram product identity.'
}
if ($version.FileVersion -ne "$($metadata.version).0") {
    throw "Executable version $($version.FileVersion) does not match $($metadata.version)."
}
$stream = [IO.File]::OpenRead($binary)
try {
    $reader = [IO.BinaryReader]::new($stream)
    if ($reader.ReadUInt16() -ne 0x5A4D) { throw 'Executable has no DOS header.' }
    $stream.Position = 0x3C
    $pe = $reader.ReadInt32()
    $stream.Position = $pe
    if ($reader.ReadUInt32() -ne 0x4550 -or $reader.ReadUInt16() -ne 0x8664) {
        throw 'Executable is not a Windows x64 PE image.'
    }
} finally {
    $stream.Dispose()
}

$smokeRoot = Join-Path $env:RUNNER_TEMP 'ayugram-smoke'
$workdir = Join-Path $smokeRoot 'profile'
New-Item -ItemType Directory -Path $workdir -Force | Out-Null
Copy-Item -LiteralPath $binary -Destination "$smokeRoot\AyuGram.exe"
Copy-Item -LiteralPath "$release\modules" -Destination $smokeRoot -Recurse
$arguments = @('-debug', '-noupdate', '-workdir', ('"{0}"' -f $workdir))
$process = Start-Process -FilePath "$smokeRoot\AyuGram.exe" -ArgumentList $arguments -WorkingDirectory $smokeRoot -PassThru
$windowShown = $false
try {
    for ($attempt = 0; $attempt -lt 60; $attempt++) {
        Start-Sleep -Milliseconds 500
        $process.Refresh()
        if ($process.HasExited) {
            throw "AyuGram exited during startup with code $($process.ExitCode)."
        }
        if ($process.MainWindowHandle -ne 0) {
            $windowShown = $true
            break
        }
    }
    if (-not $windowShown) { throw 'AyuGram did not display a window within 30 seconds.' }
    Start-Sleep -Seconds 5
    $process.Refresh()
    if ($process.HasExited) { throw 'AyuGram exited immediately after displaying its window.' }
    $quit = Start-Process -FilePath "$smokeRoot\AyuGram.exe" -ArgumentList @('-quit', '-workdir', ('"{0}"' -f $workdir)) -WorkingDirectory $smokeRoot -PassThru
    if (-not $quit.WaitForExit(10000)) { throw 'The quit command timed out.' }
    if (-not $process.WaitForExit(10000)) { throw 'AyuGram did not shut down after its quit command.' }
    if ($process.ExitCode -ne 0) { throw "AyuGram exited with code $($process.ExitCode)." }
} finally {
    $process.Refresh()
    if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force }
}

$iscc = Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'
if (-not (Test-Path -LiteralPath $iscc)) {
    $iscc = (Get-Command ISCC.exe -ErrorAction Stop).Source
}
$installerName = "AyuGram-$($metadata.version)-windows-x64-setup"
& $iscc "/dMyAppVersion=$($metadata.version)" "/dMyAppVersionFull=$($metadata.version)" `
    '/dMyBuildTarget=win64' "/dReleasePath=$release" "/dMyOutputBaseFilename=$installerName" `
    '/dUnsignedBuild=1' '/dDisableAutoUpdate=1' "$SourceRoot\Telegram\build\setup.iss"
if ($LASTEXITCODE -ne 0) { throw 'Inno Setup packaging failed.' }
$installer = Join-Path $release "$installerName.exe"
$installed = Join-Path $env:RUNNER_TEMP 'ayugram-install-smoke'
$setup = Start-Process -FilePath $installer -ArgumentList @(
    '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-', '/TASKS=', ('/DIR="{0}"' -f $installed)
) -PassThru
if (-not $setup.WaitForExit(60000)) {
    Stop-Process -Id $setup.Id -Force
    throw 'Installer smoke test timed out.'
}
if ($setup.ExitCode -ne 0 -or -not (Test-Path -LiteralPath "$installed\AyuGram.exe")) {
    throw "Installer did not install AyuGram.exe successfully (exit $($setup.ExitCode))."
}
if ((Get-FileHash -LiteralPath "$installed\AyuGram.exe" -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash) {
    throw 'Installed executable differs from the tested executable.'
}
$uninstall = Start-Process -FilePath "$installed\unins000.exe" -ArgumentList @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART') -PassThru
if (-not $uninstall.WaitForExit(60000)) {
    Stop-Process -Id $uninstall.Id -Force
    throw 'Uninstaller smoke test timed out.'
}
if ($uninstall.ExitCode -ne 0 -or (Test-Path -LiteralPath "$installed\AyuGram.exe")) {
    throw 'Uninstaller did not remove the test installation.'
}
$report = Join-Path $SourceRoot 'out\windows-smoke.json'
@{
    passed = $true
    sha256 = (Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash.ToLowerInvariant()
    installer_sha256 = (Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash.ToLowerInvariant()
    qt_version = $qtVersion
    product_name = $version.ProductName
    file_version = $version.FileVersion
    architecture = 'x64'
    window_created = $windowShown
    clean_shutdown = $true
    installer_verified = $true
    uninstaller_verified = $true
    account_login_tested = $false
    emoji_interaction_tested = $false
} | ConvertTo-Json | Set-Content -LiteralPath $report -Encoding utf8
python "$SourceRoot\Telegram\build\windows_release.py" package --root $SourceRoot `
    --output "$SourceRoot\out\windows-release" --installer $installer --smoke-report $report
if ($LASTEXITCODE -ne 0) { throw 'Release artifact validation failed.' }
