
let lastLog = null;
function fetchLogs() {
	fetch('/logs')
		.then(response => response.json())
		.then(logs => {
			const tbody = document.querySelector('#logTable tbody');
			let startIdx = 0;
			if (lastLog !== null) {
				// Find the last log in the new logs array
				startIdx = logs.findIndex(log => log === lastLog) + 1;
				if (startIdx === 0) {
					// lastLog not found, all logs are new
					startIdx = 0;
				}
			}
			for (let i = startIdx; i < logs.length; i++) {
				const row = document.createElement('tr');
				const cell = document.createElement('td');
				cell.textContent = logs[i];
				row.appendChild(cell);
				tbody.appendChild(row);
			}
			if (logs.length > 0) {
				lastLog = logs[logs.length - 1];
			}
		})
		.catch(err => {
			// Optionally show error
		});
}
// Initial fetch
fetchLogs();
// Poll every 2 seconds
setInterval(fetchLogs, 2000);
