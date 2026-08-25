# ============================================================================
#  start.ps1  --  V.I.S.O.R. Automated Dependency Checker & Launcher
# ============================================================================

$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "         V.I.S.O.R. Application Stack - One-Click Launcher" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host ""

$baseDir    = $PSScriptRoot
$pyDir      = Join-Path $baseDir "python_ai"
$pyVenv     = Join-Path $pyDir "venv"
$pyExe      = Join-Path $pyVenv "Scripts\python.exe"
$pipExe     = Join-Path $pyVenv "Scripts\pip.exe"
$reqFile    = Join-Path $pyDir "requirements.txt"
$mainPy     = Join-Path $pyDir "src\main.py"

$cppDir     = Join-Path $baseDir "cpp_render"
$buildDir   = Join-Path $cppDir "build"

# Locate visor binary in common build output locations
$visorExeCandidates = @(
    (Join-Path $buildDir "Release\VISOR.exe"),
    (Join-Path $buildDir "Release\visor.exe"),
    (Join-Path $buildDir "VISOR.exe"),
    (Join-Path $buildDir "visor.exe"),
    (Join-Path $buildDir "Debug\VISOR.exe"),
    (Join-Path $buildDir "Debug\visor.exe")
)

# ----------------------------------------------------------------------------
# 1. Check Python Installation
# ----------------------------------------------------------------------------
Write-Host "[1/4] Checking Python Environment..." -ForegroundColor Yellow

$systemPython = Get-Command python -ErrorAction SilentlyContinue
if (-not $systemPython) {
    $systemPython = Get-Command py -ErrorAction SilentlyContinue
}

if (-not $systemPython) {
    Write-Host "[FATAL ERROR] Python is not installed or not in PATH." -ForegroundColor Red
    Write-Host "Please install Python 3.10+ from https://www.python.org/downloads/ and check 'Add to PATH'." -ForegroundColor Red
    pause
    exit 1
}

Write-Host " -> Found System Python: $($systemPython.Source)" -ForegroundColor Green

# ----------------------------------------------------------------------------
# 2. Check / Create Virtual Environment & Dependencies
# ----------------------------------------------------------------------------
if (-not (Test-Path $pyExe)) {
    Write-Host " -> Virtual environment not found. Creating virtual environment at $pyVenv..." -ForegroundColor Magenta
    & $systemPython.Path -m venv $pyVenv
    if (-not (Test-Path $pyExe)) {
        Write-Host "[FATAL ERROR] Failed to create Python virtual environment." -ForegroundColor Red
        pause
        exit 1
    }
    Write-Host " -> Virtual environment created successfully." -ForegroundColor Green
}

# Verify Python dependencies in venv
Write-Host " -> Verifying Python packages (opencv-python, mediapipe, numpy)..." -ForegroundColor Gray
$needsPipInstall = $false
try {
    $testResult = & $pyExe -c "import cv2, mediapipe, numpy; print('OK')" 2>$null
    if ($testResult -ne "OK") { $needsPipInstall = $true }
} catch {
    $needsPipInstall = $true
}

if ($needsPipInstall) {
    Write-Host " -> Installing required Python dependencies from $reqFile..." -ForegroundColor Magenta
    & $pipExe install -r $reqFile
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[FATAL ERROR] Failed to install Python dependencies via pip." -ForegroundColor Red
        pause
        exit 1
    }
    Write-Host " -> Python dependencies installed successfully." -ForegroundColor Green
} else {
    Write-Host " -> All Python dependencies are satisfied." -ForegroundColor Green
}

# ----------------------------------------------------------------------------
# 3. Check / Build C++ Native Rendering Engine
# ----------------------------------------------------------------------------
Write-Host "`n[2/4] Checking C++ Native Rendering Engine..." -ForegroundColor Yellow

$targetVisorExe = $null
foreach ($candidate in $visorExeCandidates) {
    if (Test-Path $candidate) {
        $targetVisorExe = $candidate
        break
    }
}

if (-not $targetVisorExe) {
    Write-Host " -> C++ executable not found. Checking for CMake..." -ForegroundColor Magenta
    $cmakeCmd = Get-Command cmake -ErrorAction SilentlyContinue
    if (-not $cmakeCmd) {
        Write-Host "[FATAL ERROR] CMake is not installed or not found in system PATH." -ForegroundColor Red
        Write-Host "Please install CMake from https://cmake.org/download/ to build the C++ engine." -ForegroundColor Red
        pause
        exit 1
    }

    Write-Host " -> CMake detected. Configuring and compiling C++ project in Release mode..." -ForegroundColor Magenta
    if (-not (Test-Path $buildDir)) {
        New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
    }

    & cmake -B $buildDir -S $cppDir -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[FATAL ERROR] CMake configuration failed." -ForegroundColor Red
        pause
        exit 1
    }

    & cmake --build $buildDir --config Release
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[FATAL ERROR] C++ compilation failed." -ForegroundColor Red
        pause
        exit 1
    }

    # Re-check for output binary
    foreach ($candidate in $visorExeCandidates) {
        if (Test-Path $candidate) {
            $targetVisorExe = $candidate
            break
        }
    }

    if (-not $targetVisorExe) {
        Write-Host "[FATAL ERROR] C++ compilation succeeded but binary could not be found." -ForegroundColor Red
        pause
        exit 1
    }
    Write-Host " -> C++ engine compiled successfully at: $targetVisorExe" -ForegroundColor Green
} else {
    Write-Host " -> C++ executable verified at: $targetVisorExe" -ForegroundColor Green
}

# ----------------------------------------------------------------------------
# 4. Launch Services
# ----------------------------------------------------------------------------
Write-Host "`n[3/4] Launching Python AI Tracking Microservice (Port 5005)..." -ForegroundColor Yellow

$activateBat = Join-Path $pyVenv "Scripts\activate.bat"
$srcDir      = Join-Path $pyDir "src"
$cmdArgs     = "/c `"cd /d `"$srcDir`" & `"$activateBat`" & python main.py`""

# Launch hidden background process
$pythonProcess = Start-Process cmd -ArgumentList $cmdArgs -WindowStyle Hidden -PassThru

# Staging delay for webcam lock
Write-Host " -> Waiting 2 seconds for webcam hardware initialization..." -ForegroundColor Gray
Start-Sleep -Seconds 2

Write-Host "`n[4/4] Starting V.I.S.O.R 3D Viewport..." -ForegroundColor Yellow
Write-Host "----------------------------------------------------------" -ForegroundColor Cyan
Write-Host " Instructions:" -ForegroundColor White
Write-Host "  - Open Palm     : Orbit / Rotate camera" -ForegroundColor White
Write-Host "  - Peace Sign    : Pan camera horizontally / vertically" -ForegroundColor White
Write-Host "  - Pinch & Drag  : Zoom in / Zoom out" -ForegroundColor White
Write-Host "  - Fist          : Snap camera back to default view" -ForegroundColor White
Write-Host "  - Close Window  : Cleanly terminates both engine & AI backend" -ForegroundColor White
Write-Host "----------------------------------------------------------" -ForegroundColor Cyan

# Block until user closes the OpenGL viewport window
Start-Process -FilePath $targetVisorExe -WorkingDirectory $cppDir -Wait

# ----------------------------------------------------------------------------
# 5. Teardown & Process Cleanup
# ----------------------------------------------------------------------------
Write-Host "`nShutting down background services..." -ForegroundColor Yellow

if ($pythonProcess -and -not $pythonProcess.HasExited) {
    Stop-Process -Id $pythonProcess.Id -Force -ErrorAction SilentlyContinue
}

# Kill any child python worker running main.py
try {
    $wmiQuery = "CommandLine LIKE '%main.py%'"
    Get-CimInstance Win32_Process -Filter $wmiQuery -ErrorAction SilentlyContinue | ForEach-Object {
        Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue
    }
} catch {
    # Ignore cleanup errors
}

Write-Host "[INFO] V.I.S.O.R. shutdown cleanly. Have a great day!" -ForegroundColor Cyan
