const grid = document.getElementById('splitflap-grid');
let state = null;

// Polling control
const POLL_INTERVAL_MS = 2000; // fetch every 2s while active
const POLL_ACTIVE_DURATION_MS = 2 * 60 * 1000; // stop after 2 minutes
const pollStatusEl = document.getElementById('poll-status');
const toggleBtn = document.getElementById('toggle-poll');
let pollIntervalId = null;
let pollStartTime = null;

// Helper: wrap an Instruction font character with any needed centering class
function renderInstructionChar(ch) {
	let specialClass = '';
	switch (ch) {
		case ':': specialClass = 'flap-center-colon'; break;
		case '!': specialClass = 'flap-center-exclam'; break;
		case '.': specialClass = 'flap-center-dot'; break;
		case ',': specialClass = 'flap-center-comma'; break;
		case "'": specialClass = 'flap-center-quote'; break;
	}
	return `<span class="flap-instruction${specialClass ? ' ' + specialClass : ''}">${ch}</span>`;
}

function getStateName(code) {
	switch (code) {
		case 0: return 'NORMAL';
		case 1: return 'LOOK_FOR_HOME';
		case 2: return 'SENSOR_ERROR';
		case 3: return 'PANIC';
		case 4: return 'DISABLED';
		default: return 'UNKNOWN';
	}
}

function fetchState() {
	fetch('splitflap/state.json')
		.then(r => r.json())
		.then(data => {
			state = data;
			renderGrid();
		})
		.catch(() => {
			grid.innerHTML = '<div style="text-align:center;color:#c00;">Error loading state</div>';
		});
}

function renderGrid() {
	if (!state) return;
	const cols = state.display_columns;
	const rows = state.display_rows;
	grid.style.gridTemplateColumns = `repeat(${cols}, 140px)`;
	grid.style.gridTemplateRows = `repeat(${rows}, 220px)`;
	grid.innerHTML = '';
	for (const flap of state.modules) {
		const flapCharRaw = state.flaps[flap.flap_index] || '?';
		const box = document.createElement('div');
		box.className = 'flap-box';
		box.setAttribute('data-row', flap.row);
		box.setAttribute('data-col', flap.col);
		box.style.gridColumn = flap.col + 1;
		box.style.gridRow = flap.row + 1;
		const isLastFlap = (flap.row === rows - 1) && (flap.col === cols - 1);
		// Flap character only if state is NORMAL (0)
		if (flap.state === 0) {
			if (isLastFlap) {
				// Weather mapping for a-f on the LAST flap only
				switch (flapCharRaw) {
					case 'a': box.innerHTML = `<span class="flap-emoji">&#x1F525;</span>`; break; // fire
					case 'b': box.innerHTML = `<span class="flap-emoji">&#x2601;</span>`; break; // cloudy
					case 'c': box.innerHTML = `<span class="flap-emoji">&#x1F327;</span>`; break; // rain
					case 'd': box.innerHTML = `<span class="flap-emoji">&#x2744;</span>`; break; // snow
					case 'e': box.innerHTML = `<span class="flap-emoji">&#x1F32C;</span>`; break; // wind
					case 'f': box.innerHTML = `<span class="flap-emoji">&#x26C5;</span>`; break; // partly cloudy
					default:
						box.innerHTML = renderInstructionChar(flapCharRaw);
				}
			} else {
				// Normal mapping for rest of flaps
				if (flapCharRaw === 'a') {
					box.innerHTML = `<div class="flap-color-box flap-green"></div>`;
				} else if (flapCharRaw === 'b') {
					box.innerHTML = `<div class="flap-color-box flap-red"></div>`;
				} else if (flapCharRaw === 'c') { // smile
					box.innerHTML = `<span class="flap-emoji">&#x1F603;</span>`;
				} else if (flapCharRaw === 'd') { // frown
					box.innerHTML = `<span class="flap-emoji">&#x2639;</span>`;
				} else if (flapCharRaw === 'e') { // fire
					box.innerHTML = `<span class="flap-emoji">&#x1F525;</span>`;
				} else if (flapCharRaw === 'f') { // heart
					box.innerHTML = `<span class="flap-emoji">&#x1F90D;</span>`;
				} else {
					box.innerHTML = renderInstructionChar(flapCharRaw);
				}
			}
		} else {
			box.innerHTML = `<span>&nbsp;</span>`; // keep space for layout, but hide character
		}
		// Offset controls
		const offsetControls = document.createElement('div');
		offsetControls.className = 'offset-controls';
		offsetControls.innerHTML = `
			<button class="offset-btn" onclick="changeOffset(${flap.index}, -5)">&lt;&lt;</button>
			<button class="offset-btn" onclick="changeOffset(${flap.index}, -1)">&lt;</button>
			<span class="offset-value">${flap.offset}</span>
			<button class="offset-btn" onclick="changeOffset(${flap.index}, 1)">&gt;</button>
			<button class="offset-btn" onclick="changeOffset(${flap.index}, 5)">&gt;&gt;</button>
		`;
		box.appendChild(offsetControls);
		// Icon logic
		const iconDiv = document.createElement('div');
		iconDiv.className = 'icon';
		iconDiv.innerHTML = getFlapIcon(flap);
		// Tooltip
		let tooltip = getStateName(flap.state);
		if (flap.state === 0) { // NORMAL
			if (flap.moving) tooltip += ' (moving)';
		}
		iconDiv.title = tooltip;
		box.appendChild(iconDiv);
		grid.appendChild(box);
	}
}

function getFlapIcon(flap) {
	// State enum (from firmware):
	// 0 NORMAL, 1 LOOK_FOR_HOME, 2 SENSOR_ERROR, 3 PANIC, 4 STATE_DISABLED
	switch (flap.state) {
		case 0: // NORMAL
			// If moving
			if (flap.moving) {
				return `<svg viewBox="0 0 20 20" fill="#3498db"><circle cx="10" cy="10" r="9"/><path d="M8 6l5 4-5 4V6z" fill="#fff"/></svg>`;
			}
			return '';
		case 1: // LOOK_FOR_HOME
			return `<svg viewBox="0 0 20 20" fill="#f1c40f"><circle cx="10" cy="10" r="9"/><path d="M10 5l4 4-1.4 1.4L11 8.8V15H9V8.8L7.4 10.4 6 9z" fill="#fff"/></svg>`;
		case 2: // SENSOR_ERROR
			return `<svg viewBox="0 0 20 20" fill="#e74c3c"><circle cx="10" cy="10" r="9"/><path d="M9 5h2v6H9zm0 8h2v2H9z" fill="#fff"/></svg>`;
		case 3: // PANIC
			return `<svg viewBox="0 0 20 20" fill="#8e44ad"><circle cx="10" cy="10" r="9"/><path d="M5 6l5 8 5-8H5z" fill="#fff"/></svg>`;
		case 4: // STATE_DISABLED
			return `<svg viewBox="0 0 20 20" fill="#7f8c8d"><circle cx="10" cy="10" r="9"/><path d="M6 6h8v8H6z" fill="#fff"/></svg>`;
		default:
			return '';
	}
}

window.changeOffset = function (index, delta) {
	// TODO: Implement backend call to change offset
	alert(`Change offset for module ${index} by ${delta}`);
};

function stopPolling(timeout = false) {
	if (pollIntervalId !== null) {
		clearInterval(pollIntervalId);
		pollIntervalId = null;
	}
	if (timeout) {
		pollStatusEl.textContent = 'Paused after 2 minutes';
	} else {
		pollStatusEl.textContent = 'Stopped';
	}
	toggleBtn.textContent = 'Resume Polling';
}

function startPolling() {
	if (pollIntervalId !== null) {
		clearInterval(pollIntervalId);
	}
	pollStatusEl.textContent = 'Polling...';
	toggleBtn.textContent = 'Stop Polling';
	pollStartTime = Date.now();
	fetchState();
	pollIntervalId = setInterval(() => {
		const elapsed = Date.now() - pollStartTime;
		if (elapsed >= POLL_ACTIVE_DURATION_MS) {
			stopPolling(true);
			return;
		}
		fetchState();
	}, POLL_INTERVAL_MS);
}

toggleBtn.addEventListener('click', () => {
	if (pollIntervalId === null) {
		startPolling();
	} else {
		stopPolling(false);
	}
});

// Kick off polling lifecycle
startPolling();
