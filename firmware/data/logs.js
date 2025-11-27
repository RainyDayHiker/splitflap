// Splitflap Logs Viewer

class LogViewer {
	constructor() {
		this.pollingInterval = null;
		this.isPolling = false;
		this.lastLog = null;
		this.displayedLogs = [];
		this.pollIntervalMs = 2000; // 2 seconds

		this.initializeElements();
		this.setupEventListeners();
		this.loadInitialLogs();
		this.startPolling();
	}

	initializeElements() {
		this.elements = {
			logEntries: document.getElementById('logEntries'),
			clearLogs: document.getElementById('clearLogs'),
			pausePolling: document.getElementById('pausePolling'),
			status: document.getElementById('status'),
			statusText: document.getElementById('statusText'),
			totalLogs: document.getElementById('totalLogs'),
			lastUpdated: document.getElementById('lastUpdated'),
			autoRefreshStatus: document.getElementById('autoRefreshStatus')
		};
	}

	setupEventListeners() {
		this.elements.clearLogs.addEventListener('click', () => {
			this.clearDisplayedLogs();
		});

		this.elements.pausePolling.addEventListener('click', () => {
			this.togglePolling();
		});
	}

	async loadInitialLogs() {
		try {
			this.setStatus('online', 'Loading...');
			const logs = await this.fetchLogs();
			this.displayedLogs = logs;
			this.updateLogDisplay();
			this.setStatus('online', 'Connected');
		} catch (error) {
			console.error('Error loading initial logs:', error);
			this.setStatus('offline', 'Connection Error');
			this.showError('Failed to load logs. Please check your connection.');
		}
	}

	async fetchLogs() {
		const response = await fetch('/logs');
		if (!response.ok) {
			throw new Error(`HTTP error! status: ${response.status}`);
		}
		return await response.json();
	}

	startPolling() {
		if (this.isPolling) return;

		this.isPolling = true;
		this.elements.autoRefreshStatus.textContent = 'ON';
		this.elements.pausePolling.textContent = 'Pause Updates';

		this.pollingInterval = setInterval(() => {
			this.pollForNewLogs();
		}, this.pollIntervalMs);
	}

	stopPolling() {
		if (!this.isPolling) return;

		this.isPolling = false;
		this.elements.autoRefreshStatus.textContent = 'OFF';
		this.elements.pausePolling.textContent = 'Resume Updates';

		if (this.pollingInterval) {
			clearInterval(this.pollingInterval);
			this.pollingInterval = null;
		}
	}

	togglePolling() {
		if (this.isPolling) {
			this.stopPolling();
		} else {
			this.startPolling();
		}
	}

	async pollForNewLogs() {
		try {
			const logs = await this.fetchLogs();
			const newLogs = this.getNewLogsToAdd(logs);

			if (newLogs.length > 0) {
				this.appendNewLogs(newLogs);
			}
			this.setStatus('online', 'Connected');
		} catch (error) {
			console.error('Error polling for logs:', error);
			this.setStatus('offline', 'Connection Error');
		}
	}

	getNewLogsToAdd(serverLogs) {
		if (this.displayedLogs.length === 0) {
			return serverLogs;
		}

		// Find the index of the last displayed log in the server logs
		const lastDisplayedLog = this.displayedLogs[this.displayedLogs.length - 1];
		const startIdx = serverLogs.findIndex(log => log === lastDisplayedLog);

		if (startIdx === -1) {
			// Last log not found, return all server logs (circular buffer reset)
			return serverLogs;
		}

		// Return logs after the last displayed one
		return serverLogs.slice(startIdx + 1);
	}

	appendNewLogs(newLogs) {
		newLogs.forEach(log => {
			this.displayedLogs.push(log);
			const logElement = this.createLogElement(log, true);
			this.elements.logEntries.appendChild(logElement);
		});

		// Auto-scroll to bottom
		if (newLogs.length > 0) {
			this.elements.logEntries.scrollTop = this.elements.logEntries.scrollHeight;
		}

		// Update stats
		this.elements.totalLogs.textContent = this.displayedLogs.length;
		this.elements.lastUpdated.textContent = new Date().toLocaleTimeString();
	}

	updateLogDisplay() {
		this.elements.logEntries.innerHTML = '';

		if (this.displayedLogs.length === 0) {
			this.elements.logEntries.innerHTML = '<div class="loading">No logs available.</div>';
		} else {
			this.displayedLogs.forEach(log => {
				const logElement = this.createLogElement(log, false);
				this.elements.logEntries.appendChild(logElement);
			});
		}

		// Update stats
		this.elements.totalLogs.textContent = this.displayedLogs.length;
		this.elements.lastUpdated.textContent = new Date().toLocaleTimeString();
	}

	createLogElement(log, isNew = false) {
		const logDiv = document.createElement('div');
		logDiv.className = `log-entry${isNew ? ' new' : ''}`;

		const timestamp = new Date().toLocaleTimeString();

		logDiv.innerHTML = `
			<span class="log-timestamp">${timestamp}</span>
			<span class="log-message">${this.escapeHtml(log)}</span>
		`;

		return logDiv;
	}

	escapeHtml(text) {
		const div = document.createElement('div');
		div.textContent = text;
		return div.innerHTML;
	}

	clearDisplayedLogs() {
		this.elements.logEntries.innerHTML = '<div class="loading">Display cleared. Logs will continue to update...</div>';
		// Keep the displayed logs in memory for comparison
	}

	setStatus(status, text) {
		this.elements.status.className = `status-indicator ${status}`;
		this.elements.statusText.textContent = text;
	}

	showError(message) {
		this.elements.logEntries.innerHTML = `<div class="error">${this.escapeHtml(message)}</div>`;
	}
}

// Initialize the log viewer when the page loads
document.addEventListener('DOMContentLoaded', () => {
	window.logViewer = new LogViewer();
});

// Handle page visibility changes to pause/resume polling
document.addEventListener('visibilitychange', () => {
	if (window.logViewer) {
		if (document.hidden) {
			window.logViewer.stopPolling();
		} else {
			window.logViewer.startPolling();
		}
	}
});
