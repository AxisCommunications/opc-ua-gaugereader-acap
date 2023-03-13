const w = 1024;
const h = 576;
const msize = 6;
const State = {
	Begin: Symbol(0),
	Center: Symbol(1),
	Min: Symbol(2),
	Max: Symbol(3)
}
const parambaseurl = '/axis-cgi/param.cgi?action=';
const paramgeturl = parambaseurl + 'list&group=opcuagaugereader.';
const paramseturl = parambaseurl + 'update&opcuagaugereader.';
var points = {};
points['centerX'] = 0;
points['centerY'] = 0;
points['minX'] = 0;
points['minY'] = 0;
points['maxX'] = 0;
points['maxY'] = 0;

function getPointText(param) {
	return param + ': (' + points[param + 'X'] + ', ' + points[param + 'Y'] + ')';
}

function drawMarker(X, Y, color) {
	ctx.beginPath();
	ctx.fillStyle = color;
	ctx.strokeStyle = 'white';
	ctx.arc(X, Y, msize, 0, 2 * Math.PI, false);
	ctx.fill();
	ctx.arc(X, Y, msize, 1, 2 * Math.PI, false);
	ctx.stroke();
	document.getElementById("values").textContent =
		getPointText('center') + ' ' + getPointText('min') + ' ' + getPointText('max');
}

function drawCenter() {
	drawMarker(points['centerX'], points['centerY'], '#ff0033');
}

function drawMin() {
	drawMarker(points['minX'], points['minY'], '#ffcc33');
}

function drawMax() {
	drawMarker(points['maxX'], points['maxY'], '#ffcc33');
}

function drawDefaultPoints() {
	ctx.clearRect(0, 0, draw.width, draw.height);
	drawMax();
	drawCenter();
	drawMin();
}

function getCurrentValue(param) {
	$.get(paramgeturl + param)
		.done(function(data) {
			points[param] = data.split('=')[1];
			console.log('Got ' + param + ' value ' + points[param]);
			drawDefaultPoints();
		})
		.fail(function(data) {
			alert('FAILED to set ' + param);
		});
}

function setParam(param, value) {
	points[param] = value;
	$.get(paramseturl + param + '=' + value)
		.done(function(data) {
			console.log('Set ' + param + ' to ' + value);
		})
		.fail(function(jqXHR, textStatus, errorThrown) {
			alert('FAILED to set ' + param);
		});
}

function initWithCurrentValues() {
	getCurrentValue('centerX');
	getCurrentValue('centerY');
	getCurrentValue('minX');
	getCurrentValue('minY');
	getCurrentValue('maxX');
	getCurrentValue('maxY');
}

function handleCoord(X, Y) {
	switch (curState) {
		case State.Center:
			setParam('centerX', X);
			setParam('centerY', Y);
			ctx.clearRect(0, 0, draw.width, draw.height);
			drawCenter();
			curState = State.Min;
			infoTxt.innerHTML = 'Please mark gauge minimum point';
			break;
		case State.Min:
			setParam('minX', X);
			setParam('minY', Y);
			drawMin();
			curState = State.Max;
			infoTxt.innerHTML = 'Please mark gauge maximum point';
			break;
		case State.Max:
			setParam('maxX', X);
			setParam('maxY', Y);
			drawMax();
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
preview.src = '/axis-cgi/mjpg/video.cgi?resolution=' + w + 'x' + h;
var ctx = draw.getContext('2d');
initWithCurrentValues();
handleCoord(0, 0);
