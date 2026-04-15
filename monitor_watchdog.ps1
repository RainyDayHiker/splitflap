# Watchdog Monitor Script
# Monitors the splitflap for 10 minute intervals, checks for watchdog resets,
# and keeps logs only when crashes occur

$logDir = "watchdog_logs"
$monitorDuration = 600 # 10 minutes in seconds

# Create log directory if it doesn't exist
if (-not (Test-Path $logDir)) {
	New-Item -ItemType Directory -Path $logDir | Out-Null
}

Write-Host "Starting watchdog monitoring script..."
Write-Host "Monitoring for 10 minute intervals, keeping logs only when watchdog resets occur"
Write-Host "Press Ctrl+C to stop"
Write-Host ""

$iteration = 1

while ($true) {
	$timestamp = Get-Date -Format "yyyy-MM-dd_HH-mm-ss"
	$logFile = Join-Path $logDir "log_$timestamp.txt"
    
	Write-Host "[$timestamp] Starting monitoring iteration $iteration (10 minutes)..."
    
	# Start the monitor process directly (not as a job) and capture output to file
	$process = Start-Process -FilePath "C:\Users\jeffr\.platformio\penv\Scripts\platformio.exe" `
		-ArgumentList "device", "monitor", "--environment", "chainlink", "--raw" `
		-WorkingDirectory "D:\Source Code\Repos\splitflap" `
		-RedirectStandardOutput $logFile `
		-NoNewWindow `
		-PassThru
    
	if (-not $process) {
		Write-Host "Failed to start monitor process!" -ForegroundColor Red
		Start-Sleep -Seconds 5
		continue
	}
	
	Write-Host "Monitor started (PID: $($process.Id)), waiting for 10 minutes..."
	
	# Wait for the specified duration
	$endTime = (Get-Date).AddSeconds($monitorDuration)
	while ((Get-Date) -lt $endTime) {
		Start-Sleep -Seconds 10
        
		# Check if process is still running
		if ($process.HasExited) {
			Write-Host "Monitor process stopped unexpectedly (exit code: $($process.ExitCode)), restarting..."
			break
		}
	}
    
	# Stop the monitoring process
	Write-Host "Stopping monitor..."
	if (-not $process.HasExited) {
		$process.Kill()
		$process.WaitForExit(5000)
	}
    
	# Give it a moment to finish writing
	Start-Sleep -Seconds 2
    
	# Check if the log file contains watchdog reset
	if (Test-Path $logFile) {
		$watchdogFound = Select-String -Path $logFile -Pattern "task_wdt|Task watchdog|CORRUPTED|wdt reset|rst cause:4|watchdog got triggered|Aborting" -Quiet
        
		if ($watchdogFound) {
			Write-Host "*** WATCHDOG RESET DETECTED! ***" -ForegroundColor Red
			Write-Host "Log saved to: $logFile" -ForegroundColor Yellow
            
			# Extract and display context around the reset
			$context = Select-String -Path $logFile -Pattern "task_wdt|Task watchdog|CORRUPTED" -Context 50, 20 | Select-Object -Last 1
			Write-Host ""
			Write-Host "Last 50 lines before watchdog trigger:" -ForegroundColor Cyan
			if ($context) {
				Write-Host $context
			}
			else {
				# If no context found, show last 50 lines of file
				Write-Host "Showing last 50 lines of log:"
				Get-Content $logFile -Tail 50 | Write-Host
			}
			Write-Host ""
            
			# Ask user if they want to continue
			$response = Read-Host "Continue monitoring? (Y/N)"
			if ($response -ne "Y" -and $response -ne "y") {
				Write-Host "Monitoring stopped by user."
				break
			}
		}
		else {
			Write-Host "No watchdog reset detected. Deleting log file."
			Remove-Item -Path $logFile -Force
		}
	}
	else {
		Write-Host "Warning: Log file not found at $logFile"
	}
    
	$iteration++
	Write-Host ""
}

Write-Host "Monitoring script ended."
