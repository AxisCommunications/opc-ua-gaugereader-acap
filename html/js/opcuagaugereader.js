/**
 * Copyright (C) 2025, Axis Communications AB, Lund, Sweden
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

const w = 640;
const h = 360;
const msize = 5;
const State = {
	Begin: Symbol(0),
	Center: Symbol(1),
	Min: Symbol(2),
	Max: Symbol(3)
}
const paramappname = 'Opcuagaugereader';
const parambaseurl = '/axis-cgi/param.cgi?action=';
const paramgeturl = `${parambaseurl}list&group=${paramappname}.`;
const paramseturl = `${parambaseurl}update&${paramappname}.`;
var points = {};
points['centerX'] = 0;
points['centerY'] = 0;
points['minX'] = 0;
points['minY'] = 0;
points['maxX'] = 0;
points['maxY'] = 0;

function getPointText(param) {
	return `${param}: (${points[`${param}X`]}, ${points[`${param}Y`]})`;
}

function drawMarker(X, Y, color, updateText) {
	ctx.beginPath();
	ctx.fillStyle = color;
	ctx.strokeStyle = 'white';
	ctx.arc(X, Y, msize, 0, 2 * Math.PI, false);
	ctx.fill();
	ctx.arc(X, Y, msize, 1, 2 * Math.PI, false);
	ctx.stroke();

	if (updateText) {
	    updateTextDisplay();
	}
}

function updateTextDisplay() {
    document.getElementById("values").textContent =
        `${getPointText('center')} ${getPointText('min')} ${getPointText('max')}`;
}

function drawCenter(updateText) {
	drawMarker(points['centerX'], points['centerY'], '#ff0033', updateText);
}

function drawMin(updateText) {
	drawMarker(points['minX'], points['minY'], '#ffcc33', updateText);
}

function drawMax(updateText) {
	drawMarker(points['maxX'], points['maxY'], '#ffcc33', updateText);
}

function drawDefaultPoints() {
	ctx.clearRect(0, 0, draw.width, draw.height);
	drawMax(false);
	drawCenter(false);
	drawMin(true);
}

function getCurrentValue(param) {
	return fetch(`${paramgeturl}${param}`)
		.then(response => {
			if (!response.ok) {
				throw new Error(`Getting parameter value, the network response was not ok: ${response.status} ${response.statusText}`);
			}
			return response.text();
		})
		.then(data => {
			var value = data.split('=')[1];
			console.log(`Got ${param} value ${value}`);
			return Number(value);
		})
		.catch(error => {
			alert(`FAILED to get ${param}`);
			throw error;
		});
}

function setParam(param, value) {
	points[param] = value;
	fetch(`${paramseturl}${param}=${value}`)
		.then(response => {
			if (!response.ok) {
				throw new Error(`Setting parameter value, the network response was not ok: ${response.status} ${response.statusText}`);
			}
			console.log(`Set ${param} to ${value}`);
		})
		.catch(error => {
			alert(`FAILED to set ${param}: ${error.message}`);
		});
}

async function initWithCurrentValues() {
    points['centerX'] = await getCurrentValue('centerX');
    points['centerY'] = await getCurrentValue('centerY');
    points['minX'] = await getCurrentValue('minX');
    points['minY'] = await getCurrentValue('minY');
    points['maxX'] = await getCurrentValue('maxX');
    points['maxY'] = await getCurrentValue('maxY');
}

function handleCoord(X, Y) {
	switch (curState) {
		case State.Center:
			setParam('centerX', X);
			setParam('centerY', Y);
			ctx.clearRect(0, 0, draw.width, draw.height);
			drawCenter(true);
			curState = State.Min;
			infoTxt.innerHTML = 'Please mark gauge minimum point';
			break;
		case State.Min:
			setParam('minX', X);
			setParam('minY', Y);
			drawMin(true);
			curState = State.Max;
			infoTxt.innerHTML = 'Please mark gauge maximum point';
			break;
		case State.Max:
			setParam('maxX', X);
			setParam('maxY', Y);
			drawMax(true);
		default:
			curState = State.Center;
			infoTxt.innerHTML = 'Please mark gauge center point (starts new calibration)';
			break;
	}
}

function handleMouseClick(event) {
	var offsets = document.getElementById('preview').getBoundingClientRect();
	var top = offsets.top;
	var left = offsets.left;
	var X = Math.round(event.clientX - left);
	var Y = Math.round(event.clientY - top);
	handleCoord(X, Y);
}

document.addEventListener('click', handleMouseClick);
var curState = State.Begin;
var preview = document.getElementById('preview');
var draw = document.getElementById('draw');
var infoTxt = document.getElementById('info');
preview.width = preview.style.width = draw.width = w;
preview.height = preview.style.height = draw.height = h;
preview.src = `/axis-cgi/mjpg/video.cgi?resolution=${w}x${h}`;
var ctx = draw.getContext('2d');
(async () => {
    await initWithCurrentValues();
    handleCoord(0, 0);
    drawDefaultPoints();
})();
