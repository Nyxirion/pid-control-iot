//ws conection 
// Create WebSocket connection.
const socket = new WebSocket("http://192.168.4.1/");

//verification of succesfull connection 
// Connection opened
socket.addEventListener("open", (event) => {
  socket.send("Hello Server!");
});

const liquid = document.getElementById('liquid');

socket.addEventListener("message", (event) => {
    let data = JSON.parse(event.data);
    
    let now = Date.now();

    if(data.level){
        line1.append(Date.now(), data.level);
        liquid.style.bottom = data.level * 100 / 36 + '%';
        
    }
    if(data.setpoint){
        line2.append(Date.now(), data.setpoint);
        set.style.bottom = data.setpoint * 100/36 + '%';
    }

});

//plot //
//first we have to create the object 
let startTime = new Date().getTime();
let smoothie = new SmoothieChart({
    minValue: 0,
    maxValue: 36,
    grid: {
        verticalSections: 6,
    },
    labels: {
        fillStyle: '#FFFFFF',
        precision: 0,
        showIntermediateLabels:true,
        intermediateLabelSameAxis:false,
        fontSize: 12,
        enabled: true  // Asegurar que las etiquetas estén visibles
    },
    // ... otras configuraciones ...
    timestampFormatter: function (date) {
        // Calcula los segundos transcurridos desde el inicio
        let elapsedSeconds = Math.floor((date.getTime() - startTime) / 1000);
        return elapsedSeconds + 's';
    },
});
//communication with the canvas in the html
smoothie.streamTo(document.getElementById("chart"), 1000 /*1000*/);

//each line requires a TimeSeries object

let line1 = new TimeSeries();
let line2 = new TimeSeries();

// Add a random value to each line every second
setInterval(function () {
    
    line2.append(Date.now(), 6);
}, 1000);

// Add to SmoothieChart
smoothie.addTimeSeries(line1, { strokeStyle: 'rgb(87, 236, 255)', fillStyle: 'rgba(0, 255, 0, 0.4)', lineWidth: 2 });
smoothie.addTimeSeries(line2, { strokeStyle: 'rgb(255, 0, 0)', fillStyle: 'rgba(0,0,0,0)', lineWidth: 2 });

//end plot//

