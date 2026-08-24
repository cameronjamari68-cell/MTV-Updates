param(
    [Parameter(Mandatory=$true)]
    [ValidateSet("Titan", "RemotePlay")]
    [string]$Product,
    [switch]$NoPause,
    [switch]$NoOpen,
    [switch]$VerifyOnly
)

$ErrorActionPreference = "Stop"
$PackageDir = [System.IO.Path]::GetFullPath((Split-Path -Parent $MyInvocation.MyCommand.Path))
$HeliosRoot = Join-Path $env:LOCALAPPDATA "HeliosProject\Helios"
$PythonRoot = Join-Path $HeliosRoot "python"
$BaseInstall = Join-Path $PythonRoot "MTV"
$InstallDir = Join-Path $BaseInstall $Product
# The Helios pythonPath MUST be Python 3.11 (Helios's native runtime; the
# protected modules are cp311), so the shared env lives under ...\python\MTV\.henv
$EnvDir = Join-Path $PythonRoot "MTV\.henv"
$EnvPython = Join-Path $EnvDir "Scripts\python.exe"
$StateDir = Join-Path $env:LOCALAPPDATA "MTV"
$LogFile = Join-Path $StateDir ("install_" + $Product.ToLowerInvariant() + ".log")
$script:Stage = "startup"
$script:StagingDir = $null
$script:BackupDir = $null
$script:Committed = $false
$script:HadExistingInstall = $false
$script:IniPath = $null
$script:IniExisted = $false
$script:OldIniContent = $null
$script:InstallMutex = $null

function Finish([int]$Code) {
    try {
        if ($script:InstallMutex) {
            try { $script:InstallMutex.ReleaseMutex() } catch {}
            $script:InstallMutex.Dispose()
            $script:InstallMutex = $null
        }
    } catch {}
    if (-not $NoPause) { Write-Host ""; Read-Host "Press Enter to close" }
    exit $Code
}

function Restore-FailedInstall {
    # Best-effort rollback. Never replace the original error with a rollback
    # error; the transcript keeps both details when a file is locked.
    try {
        if ($script:Committed -and (Test-Path -LiteralPath $InstallDir -PathType Container)) {
            Remove-Item -LiteralPath $InstallDir -Recurse -Force -ErrorAction Stop
        }
        if ($script:BackupDir -and (Test-Path -LiteralPath $script:BackupDir -PathType Container) -and
            -not (Test-Path -LiteralPath $InstallDir)) {
            [System.IO.Directory]::Move($script:BackupDir, $InstallDir)
        }
    } catch {
        Write-Host "ROLLBACK WARNING: could not restore the previous $Product installation: $($_.Exception.Message)" -ForegroundColor Red
    }
    try {
        if ($script:IniPath -and $script:IniExisted) {
            [System.IO.File]::WriteAllText($script:IniPath, $script:OldIniContent,
                (New-Object System.Text.UTF8Encoding($false)))
        } elseif ($script:IniPath -and (Test-Path -LiteralPath $script:IniPath -PathType Leaf)) {
            Remove-Item -LiteralPath $script:IniPath -Force -ErrorAction SilentlyContinue
        }
    } catch {
        Write-Host "ROLLBACK WARNING: could not restore Helios settings: $($_.Exception.Message)" -ForegroundColor Red
    }
    try {
        if ($script:StagingDir -and (Test-Path -LiteralPath $script:StagingDir)) {
            Remove-Item -LiteralPath $script:StagingDir -Recurse -Force -ErrorAction SilentlyContinue
        }
    } catch {}
}

function Fail([object]$ErrorRecord) {
    $message = if ($ErrorRecord -is [System.Management.Automation.ErrorRecord]) {
        $ErrorRecord.Exception.Message
    } else { [string]$ErrorRecord }
    Write-Host ""
    Write-Host "INSTALL FAILED - MTV $Product" -ForegroundColor Red
    Write-Host "Stage: $script:Stage" -ForegroundColor Yellow
    Write-Host "Reason: $message" -ForegroundColor Red
    if ($ErrorRecord -is [System.Management.Automation.ErrorRecord] -and $ErrorRecord.InvocationInfo) {
        Write-Host "Location: $($ErrorRecord.InvocationInfo.PositionMessage)" -ForegroundColor DarkYellow
    }
    Write-Host "Log: $LogFile"
    Restore-FailedInstall
    try { Stop-Transcript | Out-Null } catch {}
    Finish 1
}

function Set-Stage([string]$Name) {
    $script:Stage = $Name
    Write-Host "[Stage] $Name" -ForegroundColor DarkCyan
}

function Invoke-LoggedNative {
    param(
        [Parameter(Mandatory=$true)][string]$Description,
        [Parameter(Mandatory=$true)][scriptblock]$Command
    )
    $previous = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $output = @(& $Command 2>&1)
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
    }
    foreach ($line in $output) { Write-Host ([string]$line) }
    if ($exitCode -ne 0) {
        $detail = (($output | ForEach-Object { [string]$_ }) -join "`n").Trim()
        if ($detail.Length -gt 3000) { $detail = $detail.Substring($detail.Length - 3000) }
        throw "$Description failed (exit code $exitCode). Output:`n$detail"
    }
}

function Assert-ChildPath([string]$Path, [string]$Parent) {
    $resolved = [System.IO.Path]::GetFullPath($Path)
    $parentResolved = [System.IO.Path]::GetFullPath($Parent) + [System.IO.Path]::DirectorySeparatorChar
    if (-not $resolved.StartsWith($parentResolved, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing unsafe install path: $resolved"
    }
}

function Test-GeneratedPythonCache([string]$Path) {
    $normalized = $Path.Replace('/', '\')
    return ($normalized -match '(?i)\\__pycache__\\' -or [System.IO.Path]::GetExtension($normalized) -in @('.pyc', '.pyo'))
}

function Test-PackageRuntimeState([string]$Path) {
    $normalized = $Path.Replace('/', '\')
    # mtv_live.json is generated beside the launcher when somebody runs the
    # extracted package before installing. It is telemetry, not release
    # payload, so its presence must not make Install/Repair reject the package.
    return ($normalized -in @("mtv_live.json", "mtv_gui.log", "shot_log.jsonl") -or
        $normalized -match '(?i)^mtv_(?:config|live)\.json\.\d+\.tmp$')
}

function Test-ValidJsonObject([string]$Path) {
    try {
        $value = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
        return ($null -ne $value -and $null -ne $value.PSObject)
    }
    catch { return $false }
}

function Test-ValidPe([string]$Path) {
    # True when the file is a plausible Windows executable ("MZ" header).
    # Used to tell a real launcher swap apart from a truncated/corrupt file.
    try {
        $stream = [System.IO.File]::OpenRead($Path)
        try {
            $buf = New-Object byte[] 2
            if ($stream.Read($buf, 0, 2) -lt 2) { return $false }
            return ($buf[0] -eq 0x4D -and $buf[1] -eq 0x5A)
        }
        finally { $stream.Dispose() }
    }
    catch { return $false }
}

function Get-Sha256Hex([string]$Path) {
    $stream = $null
    $sha = $null
    try {
        $stream = [System.IO.File]::OpenRead($Path)
        $sha = [System.Security.Cryptography.SHA256]::Create()
        return ([System.BitConverter]::ToString($sha.ComputeHash($stream))).Replace("-", "").ToLowerInvariant()
    }
    finally {
        if ($stream) { $stream.Dispose() }
        if ($sha) { $sha.Dispose() }
    }
}

function Verify-IntegrityManifest([string]$Root) {
    $manifest = Join-Path $Root "SHA256SUMS.txt"
    if (-not (Test-Path -LiteralPath $manifest -PathType Leaf)) {
        throw "Package integrity manifest is missing. Re-extract a fresh complete package."
    }
    $rootPrefix = [System.IO.Path]::GetFullPath($Root).TrimEnd('\') + '\'
    $listed = @{}
    $depsWarnings = @()
    $script:BundledHeliosBad = $false
    foreach ($line in Get-Content -LiteralPath $manifest) {
        if ($line -notmatch '^([0-9a-fA-F]{64})  (.+)$') { throw "Package integrity manifest is malformed." }
        $expected = $Matches[1].ToLowerInvariant()
        $relative = $Matches[2]
        if ($relative -eq "SHA256SUMS.txt" -or $listed.ContainsKey($relative)) {
            throw "Package integrity manifest contains an invalid entry."
        }
        $full = [System.IO.Path]::GetFullPath((Join-Path $Root $relative.Replace('/', '\')))
        if (-not $full.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Package integrity manifest contains an unsafe path."
        }
        $isDeps = $relative.StartsWith("deps/", [System.StringComparison]::OrdinalIgnoreCase)
        if (-not (Test-Path -LiteralPath $full -PathType Leaf)) {
            # A missing bundled fallback (deps/) must not block an otherwise
            # valid install - see the mismatch branch below.
            if ($isDeps) {
                $depsWarnings += "missing: $relative"
                if ($relative -eq "deps/helios/Helios.exe") { $script:BundledHeliosBad = $true }
                continue
            }
            throw @"
Package file is missing: $relative

The package is incomplete - a file was not extracted. Delete this ENTIRE
product folder, re-download the full .zip, and extract it to a normal local
folder (NOT OneDrive/Dropbox/Google Drive). Do not copy single files out of
the zip.
"@
        }
        $actual = Get-Sha256Hex $full
        if ($actual -ne $expected) {
            # Settings and telemetry are mutable runtime state, not executable
            # payload. If Helios was accidentally pointed at the extracted
            # package, accept valid JSON so Repair can recover the installation.
            if ($relative -in @("mtv_config.json", "mtv_live.json") -and
                (Test-ValidJsonObject $full)) {
                Write-Host "Package runtime state changed: $relative (accepted for repair)." -ForegroundColor Yellow
            }
            elseif ($isDeps) {
                # Bundled 3rd-party fallbacks under deps/ (Helios for machines
                # that lack it, ViGEm for the optional virtual pad) are only
                # copied when those host components are missing and are never
                # part of the protected install. A stale or merged-over copy
                # must not block an otherwise-valid install, so this is a
                # warning, not a failure - the product payload (everything
                # above deps/) still hard-fails on any mismatch.
                $depsWarnings += "mismatch: $relative"
                if ($relative -eq "deps/helios/Helios.exe") { $script:BundledHeliosBad = $true }
            }
            elseif ($relative -eq "MTVInstaller.exe" -and (Test-ValidPe $full)) {
                # MTVInstaller.exe is the hub-managed launcher, not part of
                # the protected install. The hub legitimately swaps it during
                # its self-update and patches SHA256SUMS.txt to match; if that
                # swap and the manifest patch do not land together (a crash
                # between the two, or a publisher rebuild that only updated
                # the exe), the manifest can be one version behind the file.
                # A valid PE is therefore accepted with a warning instead of
                # bricking every install with an opaque "exit code 1" -- the
                # product payload above still hard-fails on any real damage.
                Write-Host "Package runtime state changed: $relative (valid launcher; accepted for install)." -ForegroundColor Yellow
            }
            else {
                $shortExpected = $expected.Substring(0, 12)
                $shortActual = $actual.Substring(0, 12)
                throw @"
Package integrity check failed: $relative
  expected sha256 ...$shortExpected
  found    sha256 ...$shortActual

This file on this PC does not match the original release, so it was damaged
after download - usually an interrupted download, a partial unzip, or a
cloud-sync folder (OneDrive/Dropbox/Google Drive) rewriting it.

To fix it:
  1. Delete this ENTIRE product folder.
  2. Download the full .zip again and confirm the download completes.
  3. Extract it to a normal local folder - NOT OneDrive/Dropbox/Google Drive.
  4. Run INSTALL_OR_REPAIR_MTV.cmd from the extracted folder.

Do not copy single files out of the zip, and do not run the installer from
inside the zip.
"@
            }
        }
        $listed[$relative] = $true
    }
    foreach ($file in Get-ChildItem -LiteralPath $Root -Recurse -File) {
        if ($file.FullName -eq $manifest) { continue }
        $relative = $file.FullName.Substring($rootPrefix.Length).Replace('\', '/')
        # Launching a Python entry point directly from the extracted package
        # may create harmless, unlisted cache files. They do not alter any
        # signed release input and must not prevent repair/reinstallation.
        if (Test-GeneratedPythonCache $relative) { continue }
        if (Test-PackageRuntimeState $relative) { continue }
        if (-not $listed.ContainsKey($relative)) {
            # A stray file under deps/ (for example an older Helios folder left
            # over by merging into a previous extraction) is not part of the
            # protected install and must not block it.
            if ($relative.StartsWith("deps/", [System.StringComparison]::OrdinalIgnoreCase)) {
                $depsWarnings += "unlisted: $relative"
                continue
            }
            throw "Unlisted file found in package: $relative"
        }
    }
    if ($depsWarnings.Count -gt 0) {
        Write-Host ""
        Write-Host "Warning: bundled components did not fully verify and may be skipped:" -ForegroundColor Yellow
        foreach ($w in ($depsWarnings | Select-Object -First 6)) {
            Write-Host "  - $w" -ForegroundColor Yellow
        }
        if ($depsWarnings.Count -gt 6) {
            Write-Host "  - ... and $($depsWarnings.Count - 6) more" -ForegroundColor Yellow
        }
        Write-Host "This usually happens when an old/partial extract is merged over a new" -ForegroundColor Yellow
        Write-Host "one (for example extracting the zip into a folder that already has an" -ForegroundColor Yellow
        Write-Host "older MTV release). It only disables the bundled Helios/ViGEm fallback;" -ForegroundColor Yellow
        Write-Host "an already-installed Helios or ViGEmBus is still used normally." -ForegroundColor Yellow
        Write-Host ""
    }
    else {
        Write-Host "Package SHA-256 integrity verified." -ForegroundColor Green
    }
}

function Merge-PreservedConfig([string]$BaselinePath, [string]$SavedJson) {
    try {
        $baseline = Get-Content -LiteralPath $BaselinePath -Raw | ConvertFrom-Json
        $saved = $SavedJson | ConvertFrom-Json
        foreach ($property in @($baseline.PSObject.Properties)) {
            if ($property.Name -like "_live_*") { continue }
            # Preserve user settings, including HUD visibility, meter tuning,
            # controller choice, and release values. Only transient _live_*
            # telemetry is intentionally omitted below.
            $old = $saved.PSObject.Properties[$property.Name]
            if ($null -ne $old) { $property.Value = $old.Value }
        }
        foreach ($property in @($baseline.PSObject.Properties | Where-Object { $_.Name -like "_live_*" })) {
            $baseline.PSObject.Properties.Remove($property.Name)
        }
        $json = $baseline | ConvertTo-Json -Depth 8
        [System.IO.File]::WriteAllText($BaselinePath, $json, (New-Object System.Text.UTF8Encoding($false)))
        Write-Host "Preserved compatible product settings; retired settings were removed."
    }
    catch {
        Write-Host "Existing settings were invalid; installed the clean release defaults." -ForegroundColor Yellow
    }
}

function Set-IniValue([string]$Path, [string]$Section, [string]$Key, [string]$Value) {
    # Rewrite one key=value inside a .ini section, preserving all other
    # settings. Appends the section+key if it does not exist yet.
    $lines = New-Object System.Collections.Generic.List[string]
    if (Test-Path -LiteralPath $Path -PathType Leaf) {
        foreach ($l in Get-Content -LiteralPath $Path) { $lines.Add([string]$l) }
    }
    $inSection = $false
    $found = $false
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $t = $lines[$i].Trim()
        if ($t -eq "[$Section]") { $inSection = $true; continue }
        if ($t.StartsWith("[")) { $inSection = $false; continue }
        if ($inSection -and $t -match ('^' + [regex]::Escape($Key) + '\s*=')) {
            $lines[$i] = "$Key=$Value"
            $found = $true
            break
        }
    }
    if (-not $found) {
        if ($lines.Count -gt 0 -and $lines[$lines.Count - 1].Trim() -ne "") { $lines.Add("") }
        $lines.Add("[$Section]")
        $lines.Add("$Key=$Value")
    }
    Set-Content -LiteralPath $Path -Value $lines -Encoding UTF8
}

try {
    New-Item -ItemType Directory -Path $StateDir -Force | Out-Null
    Start-Transcript -LiteralPath $LogFile -Force | Out-Null
    Write-Host "MTV $Product - Install or Repair" -ForegroundColor Cyan
    try {
        $script:InstallMutex = New-Object System.Threading.Mutex($false, "MTV-Install-Lock")
        $hasLock = $false
        try { $hasLock = $script:InstallMutex.WaitOne(0) }
        catch [System.Threading.AbandonedMutexException] { $hasLock = $true }
        if (-not $hasLock) {
            throw "Another MTV installer is already running. Wait for it to finish, then retry."
        }
    } catch {
        throw "Installer lock unavailable: $($_.Exception.Message)"
    }
    Set-Stage "package validation"
    Assert-ChildPath $InstallDir $BaseInstall
    Verify-IntegrityManifest $PackageDir

    $requirements = Join-Path $PackageDir "requirements-runtime.txt"
    foreach ($required in @("mtv_config.json", "mtv_launcher.py", "requirements-runtime.txt", "runtime.dat", "mtv_vision.cp311-win_amd64.pyd")) {
        if ($Product -eq "RemotePlay" -and $required -in @("runtime.dat", "mtv_vision.cp311-win_amd64.pyd")) { continue }
        if (-not (Test-Path -LiteralPath (Join-Path $PackageDir $required) -PathType Leaf)) {
            throw "Package is missing $required"
        }
    }
    if (-not (Test-Path -LiteralPath (Join-Path $PackageDir "meters") -PathType Container)) {
        throw "Package is missing the meters folder"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $PackageDir "mtv_license\core.cp311-win_amd64.pyd") -PathType Leaf)) {
        throw "Package is missing the protected license checker"
    }
    if ($Product -eq "Titan") {
        foreach ($required in @("MTVBridge.py", "_mtvbridge_core.cp311-win_amd64.pyd", "_mtv_engine.cp311-win_amd64.pyd")) {
            if (-not (Test-Path -LiteralPath (Join-Path $PackageDir $required) -PathType Leaf)) {
                throw "Titan package is missing $required"
            }
        }
    } else {
        foreach ($required in @("MTVRemotePlay.py", "remoteplay\_mtvbridge_core.cp311-win_amd64.pyd", "remoteplay\_mtv_engine.cp311-win_amd64.pyd", "remoteplay\_mtvremoteplay_core.cp311-win_amd64.pyd", "remoteplay\mtv_vision.cp311-win_amd64.pyd", "remoteplay\runtime.dat", "remoteplay\_vigem.cp311-win_amd64.pyd", "remoteplay\_xinput.cp311-win_amd64.pyd", "remoteplay\_sdl_input.cp311-win_amd64.pyd")) {
            if (-not (Test-Path -LiteralPath (Join-Path $PackageDir $required) -PathType Leaf)) {
                throw "Remote Play package is missing $required"
            }
        }
        if (-not (Test-Path -LiteralPath (Join-Path $PackageDir "remoteplay\ViGEmClient.dll") -PathType Leaf)) {
            throw "Remote Play package is missing ViGEmClient.dll"
        }
    }

    $privateNames = @("core.py", "keygen.py", "private_key.pem", "public_key.pem", "license_config.json", "bot.py", "bot_config.json", "bot_config.example.json", "deploy.ps1", "licenses.db", "Dockerfile", "fly.toml", "railway.json", "Procfile", "requirements.txt", "_mtvbridge_core.py", "_mtv_engine.py", "_mtvremoteplay_core.py", "mtv_vision.py", "_vigem.py", "_xinput.py", "_sdl_input.py", "_inspect_meter.py", "bridge.py", "input.py", "output.py", "state.py", "_diagnose_core.py")
    foreach ($name in $privateNames) {
        if (Get-ChildItem -LiteralPath $PackageDir -Recurse -File -Filter $name -ErrorAction SilentlyContinue) {
            throw "Unsafe package contains private source: $name"
        }
    }
    $privateBytecodeStems = @("core", "keygen", "bot", "_mtvbridge_core", "_mtv_engine", "_mtvremoteplay_core", "mtv_vision", "_vigem", "_xinput", "_sdl_input", "_inspect_meter", "bridge", "input", "output", "state", "_diagnose_core")
    foreach ($bytecode in Get-ChildItem -LiteralPath $PackageDir -Recurse -File -ErrorAction SilentlyContinue | Where-Object { $_.Extension -in @(".pyc", ".pyo") }) {
        $stem = $bytecode.Name.Split('.')[0]
        if ($stem -in $privateBytecodeStems) {
            throw "Unsafe package contains private implementation bytecode: $($bytecode.Name)"
        }
    }

    if ($VerifyOnly) {
        Write-Host "Package verification complete; no files were installed." -ForegroundColor Green
        Stop-Transcript | Out-Null
        Finish 0
    }

    Set-Stage "Helios shutdown check"
    $active = Get-Process -ErrorAction SilentlyContinue | Where-Object {
        $_.ProcessName -match '^(HeliosApp|python|pythonw)$' -and $_.Path -match 'Helios'
    }
    if ($active) {
        Write-Host "Helios is running - closing it before install..." -ForegroundColor Yellow
        foreach ($proc in $active) {
            Write-Host "  Stopping $($proc.ProcessName) (PID $($proc.Id))"
        }
        $active | Stop-Process -Force -ErrorAction SilentlyContinue
        $deadline = (Get-Date).AddSeconds(10)
        while ((Get-Date) -lt $deadline) {
            $still = Get-Process -ErrorAction SilentlyContinue | Where-Object {
                $_.ProcessName -match '^(HeliosApp|python|pythonw)$' -and $_.Path -match 'Helios'
            }
            if (-not $still) { break }
            Start-Sleep -Milliseconds 200
        }
        if ($still) {
            $names = ($still | ForEach-Object { "$($_.ProcessName) (PID $($_.Id))" }) -join ", "
            throw "Helios could not be closed (still running: $names). Close it manually and retry."
        }
        Write-Host "Helios closed." -ForegroundColor Green
    }

    $depsDir = Join-Path $PackageDir "deps"

    New-Item -ItemType Directory -Path $BaseInstall -Force | Out-Null
    $existingVer = $null
    $existingEnvIs311 = $false
    if (Test-Path -LiteralPath $EnvPython -PathType Leaf) {
        $existingVer = ((& $EnvPython -c "import sys;print('%d.%d' % sys.version_info[:2])" 2>$null) | Select-Object -First 1)
        if ($existingVer -and $existingVer.Trim() -like "3.11*") {
            $existingEnvIs311 = $true
        } else {
            $shownVer = if ($existingVer) { $existingVer.Trim() } else { "unusable" }
            Write-Host "Existing MTV Python is $shownVer; recreating with Python 3.11..." -ForegroundColor Yellow
            Assert-ChildPath $EnvDir $PythonRoot
            Remove-Item -LiteralPath $EnvDir -Recurse -Force
        }
    }

    # Do not select the old MTV env as the venv creation interpreter when it
    # was the wrong Python version; that path is removed just above.
    $candidates311 = @()
    if ($existingEnvIs311) { $candidates311 += $EnvPython }
    $managed311 = Get-ChildItem -LiteralPath (Join-Path $PythonRoot "managed-python") -Directory -Filter "cpython-3.11*-windows-x86_64-none" -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending | ForEach-Object { Join-Path $_.FullName "python.exe" } |
        Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
    if ($managed311) { $candidates311 += $managed311 }
    $candidates311 += (Join-Path $env:LOCALAPPDATA "Programs\Python\Python311\python.exe")
    $Py311 = $candidates311 | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
    if (-not $Py311) { throw "Python 3.11 64-bit was not found (run Helios once so it provisions Python 3.11)." }

    Set-Stage "Python 3.11 environment"
    if (-not (Test-Path -LiteralPath $EnvPython -PathType Leaf)) {
        $uv = Join-Path $PythonRoot "uv.exe"
        if (Test-Path -LiteralPath $uv -PathType Leaf) {
            Invoke-LoggedNative "Creating the shared MTV Python environment with uv" { & $uv venv --python $Py311 $EnvDir }
        } else {
            Invoke-LoggedNative "Creating the shared MTV Python environment with venv" { & $Py311 -m venv $EnvDir }
        }
    }

    # Hard guarantee: the Helios pythonPath must resolve to Python 3.11.
    $envVer = ((& $EnvPython -c "import sys;print('%d.%d' % sys.version_info[:2])" 2>$null) | Select-Object -First 1)
    if (-not $envVer -or $envVer.Trim() -notlike "3.11*") {
        throw "MTV Python env is not 3.11 (got: '$envVer'). The Helios pythonPath would be wrong."
    }
    Write-Host "MTV Python verified: $($envVer.Trim())" -ForegroundColor Green

    Set-Stage "dependency installation"
    Write-Host "Installing shared Python requirements..." -ForegroundColor Yellow
    $uv = Join-Path $PythonRoot "uv.exe"
    if (Test-Path -LiteralPath $uv -PathType Leaf) {
        Invoke-LoggedNative "Installing pinned Python requirements with uv" { & $uv pip install --python $EnvPython --require-hashes --requirements $requirements }
    } else {
        Invoke-LoggedNative "Bootstrapping pip" { & $EnvPython -m ensurepip --upgrade }
        Invoke-LoggedNative "Installing pinned Python requirements with pip" { & $EnvPython -m pip install --require-hashes --requirement $requirements }
    }

    Set-Stage "staging product files"
    $existingConfig = Join-Path $InstallDir "mtv_config.json"
    if (Test-Path -LiteralPath $existingConfig -PathType Leaf) {
        $savedConfig = Get-Content -LiteralPath $existingConfig -Raw
    }

    # Build the complete replacement beside the live install. The old copy is
    # not touched until every staged file has been copied and import-validated.
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMddHHmmssfff")
    $script:StagingDir = Join-Path $BaseInstall (".staging-" + $Product + "-" + $stamp)
    $script:BackupDir = Join-Path $BaseInstall (".backup-" + $Product + "-" + $stamp)
    Assert-ChildPath $script:StagingDir $BaseInstall
    Assert-ChildPath $script:BackupDir $BaseInstall
    New-Item -ItemType Directory -Path $script:StagingDir -Force | Out-Null

    foreach ($item in Get-ChildItem -LiteralPath $PackageDir -Force) {
        if ($item.Name -notin @("INSTALL_OR_REPAIR_MTV.cmd", "install_product.ps1", "SHA256SUMS.txt", "deps", "mtv_live.json", "mtv_gui.log", "shot_log.jsonl") -and
            $item.Name -notmatch '(?i)^mtv_(?:config|live)\.json\.\d+\.tmp$') {
            Copy-Item -LiteralPath $item.FullName -Destination $script:StagingDir -Recurse -Force
        }
    }
    if ($savedConfig) {
        $cfgPath = Join-Path $script:StagingDir "mtv_config.json"
        Merge-PreservedConfig $cfgPath $savedConfig
    }

    $scriptName = "MTVRemotePlay.py"
    if ($Product -eq "Titan") { $scriptName = "MTVBridge.py" }
    Set-Stage "staged import validation"
    $env:MTV_VALIDATION_ROOT = $script:StagingDir
    if ($Product -eq "Titan") {
        Invoke-LoggedNative "Protected Titan import validation" { & $EnvPython -c "import os,sys;root=os.environ['MTV_VALIDATION_ROOT'];sys.path.insert(0,root);import MTVBridge;from mtv_vision import VisionLocalizer;from mtv_license import core;v=VisionLocalizer({});assert callable(MTVBridge.process);assert callable(core.is_unlocked);assert v.available or 'license required' in str(v.error);print('Protected Titan staged verification passed')" }
    } else {
        Invoke-LoggedNative "Protected Remote Play import validation" { & $EnvPython -c "import os,sys;root=os.environ['MTV_VALIDATION_ROOT'];sys.path.insert(0,root);import remoteplay,MTVRemotePlay;from remoteplay.mtv_vision import VisionLocalizer;from mtv_license import core;v=VisionLocalizer({});assert callable(MTVRemotePlay.process);assert callable(remoteplay.start_bridge);assert callable(core.is_unlocked);assert v.available or 'license required' in str(v.error);print('Protected Remote Play staged verification passed')" }
    }
    Remove-Item Env:MTV_VALIDATION_ROOT -ErrorAction SilentlyContinue

    Set-Stage "atomic product commit"
    $script:HadExistingInstall = Test-Path -LiteralPath $InstallDir -PathType Container
    if (Test-Path -LiteralPath $InstallDir -PathType Leaf) {
        throw "Install destination exists as a file, not a directory: $InstallDir"
    }
    if ($script:HadExistingInstall) {
        [System.IO.Directory]::Move($InstallDir, $script:BackupDir)
    }
    try {
        [System.IO.Directory]::Move($script:StagingDir, $InstallDir)
        $script:Committed = $true
    } catch {
        if ($script:HadExistingInstall -and (Test-Path -LiteralPath $script:BackupDir -PathType Container) -and
            -not (Test-Path -LiteralPath $InstallDir)) {
            [System.IO.Directory]::Move($script:BackupDir, $InstallDir)
        }
        throw
    }
    $scriptPath = Join-Path $InstallDir $scriptName

    $heliosAppDir = Join-Path $env:LOCALAPPDATA "Programs\Helios"
    $heliosExe = Join-Path $heliosAppDir "Helios.exe"
    $helioPkg = Join-Path $depsDir "helios"
    # Never install a bundled Helios whose integrity already failed (corrupt /
    # stale copy) - it would only replace a missing Helios with a broken one.
    $bundledHeliosOk = (-not $script:BundledHeliosBad)
    if ($bundledHeliosOk -and -not (Test-Path -LiteralPath $heliosExe -PathType Leaf) -and (Test-Path -LiteralPath (Join-Path $helioPkg "Helios.exe") -PathType Leaf)) {
        Write-Host "Installing bundled Helios..." -ForegroundColor Yellow
        New-Item -ItemType Directory -Path $heliosAppDir -Force | Out-Null
        Copy-Item -Path (Join-Path $helioPkg "*") -Destination $heliosAppDir -Recurse -Force
    }

    if (Test-Path -LiteralPath $heliosExe -PathType Leaf) {
        $script:IniPath = Join-Path $heliosAppDir "helios_settings.ini"
        $script:IniExisted = Test-Path -LiteralPath $script:IniPath -PathType Leaf
        if ($script:IniExisted) {
            $script:OldIniContent = Get-Content -LiteralPath $script:IniPath -Raw
        }
        # Always (re)point Helios at the MTV Python 3.11 env + the right script,
        # even when Helios was already installed.  This is the fix for
        # "ModuleNotFoundError: No module named '_mtvbridge_core'" on machines
        # whose Helios still points at an older Python (for example 3.13).
        $ini = $script:IniPath
        $slot = "0"
        if ($Product -eq "RemotePlay") { $slot = "1" }
        $envPyFwd = $EnvPython.Replace("\", "/")
        $scriptFwd = $scriptPath.Replace("\", "/")

        if (-not (Test-Path -LiteralPath $ini -PathType Leaf)) {
            $template = Join-Path $helioPkg "helios_settings.ini"
            if (Test-Path -LiteralPath $template -PathType Leaf) {
                $content = (Get-Content -LiteralPath $template -Raw).Replace("__MTV_PYTHON_PATH__", $envPyFwd).Replace("__MTV_SCRIPT_PATH__", $scriptFwd).Replace("__MTV_CONTROLLER_SLOT__", $slot)
                Set-Content -LiteralPath $ini -Value $content -Encoding UTF8
            } else {
                # A host Helios install may not contain the bundled template.
                # Create the minimum settings instead of claiming integration
                # succeeded while leaving Helios pointed at an old interpreter.
                Set-IniValue $ini "Application" "pythonPath" $envPyFwd
                Set-IniValue $ini "cv_python" "selectedScript" $scriptFwd
                Set-IniValue $ini "XInputInput" "selectedControllerId" $slot
            }
        } else {
            Set-IniValue $ini "Application" "pythonPath" $envPyFwd
            Set-IniValue $ini "cv_python" "selectedScript" $scriptFwd
            Set-IniValue $ini "XInputInput" "selectedControllerId" $slot
        }
        if ($Product -eq "RemotePlay") {
            # Helios labels its zero-based DS5 id 0 as "Controller 1". Make
            # that device the initial DS5 selection. Preload the GPC3 source
            # into slot 0 only when the customer has no script there already;
            # never erase a working/custom GPC3 slot during repair.
            $gpc3Path = (Join-Path $InstallDir "MTVRemotePlay.gpc3").Replace("\", "/")
            Set-IniValue $ini "DS5Input" "selectedControllerId" "0"
            Set-IniValue $ini "DS5Input" "pollingRate" "250"
            $gpc3SlotLine = Get-Content -LiteralPath $ini | Where-Object { $_ -match '^\s*slotScripts\s*=' } | Select-Object -First 1
            if (-not $gpc3SlotLine -or $gpc3SlotLine -match '^\s*slotScripts\s*=\s*(?:,\s*)?$') {
                Set-IniValue $ini "gpc3" "slotScripts" ($gpc3Path + ", ")
                Set-IniValue $ini "gpc3" "slotTranslators" ", "
            }
            Set-IniValue $ini "gpc3" "activeSlot" "0"
        }
        Write-Host "Helios now uses the MTV Python 3.11 environment:" -ForegroundColor Green
        Write-Host "  $envPyFwd" -ForegroundColor Cyan

        $desktop = [Environment]::GetFolderPath("Desktop")
        $shortcut = Join-Path $desktop "MTV Helios.lnk"
        if (-not (Test-Path -LiteralPath $shortcut -PathType Leaf)) {
            try {
                $sh = New-Object -ComObject WScript.Shell
                $lnk = $sh.CreateShortcut($shortcut)
                $lnk.TargetPath = $heliosExe
                $lnk.WorkingDirectory = $heliosAppDir
                $lnk.Save()
                Write-Host "Desktop shortcut created: MTV Helios" -ForegroundColor Green
            } catch {}
        }
    } else {
        Write-Host "NOTE: Helios was not found and is not bundled. In Helios, set" -ForegroundColor Yellow
        Write-Host "  CV Python -> pythonPath = $EnvPython" -ForegroundColor Cyan
    }

    Set-Stage "installed import validation"
    $env:MTV_VALIDATION_ROOT = $InstallDir
    if ($Product -eq "Titan") {
        Invoke-LoggedNative "Installed Titan import validation" { & $EnvPython -c "import os,sys;root=os.environ['MTV_VALIDATION_ROOT'];sys.path.insert(0,root);import MTVBridge;from mtv_vision import VisionLocalizer;from mtv_license import core;v=VisionLocalizer({});assert callable(MTVBridge.process);assert callable(core.is_unlocked);assert v.available or 'license required' in str(v.error);print('Protected Titan verified')" }
    } else {
        Invoke-LoggedNative "Installed Remote Play import validation" { & $EnvPython -c "import os,sys;root=os.environ['MTV_VALIDATION_ROOT'];sys.path.insert(0,root);import remoteplay,MTVRemotePlay;from remoteplay.mtv_vision import VisionLocalizer;from mtv_license import core;v=VisionLocalizer({});assert callable(MTVRemotePlay.process);assert callable(remoteplay.start_bridge);assert callable(core.is_unlocked);assert v.available or 'license required' in str(v.error);print('Protected Remote Play verified')" }
    }
    Remove-Item Env:MTV_VALIDATION_ROOT -ErrorAction SilentlyContinue

    if ($Product -eq "RemotePlay") {
        $vigem = Get-Service -Name "ViGEmBus" -ErrorAction SilentlyContinue
        if (-not $vigem) {
            $vigemDir = Join-Path $depsDir "vigem"
            $inf = Join-Path $vigemDir "ViGEmBus.inf"
            $devcon = Join-Path $vigemDir "devcon.exe"
            if ((Test-Path -LiteralPath $inf -PathType Leaf) -and (Test-Path -LiteralPath $devcon -PathType Leaf)) {
                Write-Host "Installing ViGEmBus driver (approve the UAC prompt)..." -ForegroundColor Yellow
                $dp = Start-Process -FilePath $devcon -ArgumentList "install `"$inf`" `"Nefarius\ViGEmBus\Gen1`"" -Verb RunAs -Wait -PassThru -WorkingDirectory $vigemDir -WindowStyle Hidden
                Write-Host "ViGEmBus driver installer finished (exit $($dp.ExitCode))."
            } else {
                Write-Host "NOTE: ViGEmBus driver was not bundled and is not installed." -ForegroundColor Yellow
                Write-Host "Install it from https://github.com/nefarius/ViGEmBus/releases and reboot."
            }
        } else {
            Write-Host "ViGEmBus driver: found" -ForegroundColor Green
        }
    }

    Set-Stage "cleanup"
    try {
        if ($script:BackupDir -and (Test-Path -LiteralPath $script:BackupDir -PathType Container)) {
            Remove-Item -LiteralPath $script:BackupDir -Recurse -Force -ErrorAction Stop
        }
    } catch {
        # Keeping the backup is safer than failing a completed install; it is
        # harmless and gives support a recovery copy if an antivirus lock
        # briefly prevents cleanup.
        Write-Host "Warning: previous $Product backup was kept at $script:BackupDir" -ForegroundColor Yellow
    }
    try { Set-Clipboard -Value $scriptPath } catch {}
    Write-Host ""; Write-Host "INSTALL COMPLETE" -ForegroundColor Green
    Write-Host "Helios script:" -ForegroundColor Cyan; Write-Host $scriptPath
    Write-Host "Next: open START_HERE.txt in this folder for the 3-step setup." -ForegroundColor Cyan
    Write-Host "Both products use the same license stored under: $StateDir"
    Stop-Transcript | Out-Null
    if (-not $NoOpen) { Start-Process explorer.exe -ArgumentList $InstallDir }
    Finish 0
}
catch { Fail $_.Exception.Message }
