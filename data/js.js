window.addEventListener("load", () => {
    //reference we need 
    const liquid = document.getElementById("liquid");
    const readout = document.getElementById("readout");
    const setpoint = document.getElementById("setpoint");

    const range_setpoint = document.getElementById("st");
    const setpoint_readout = document.querySelector(".setpoint-value");

    //create websocket connection
    const socket = new WebSocket("http://192.168.4.1/ws");

    //Listen for messages
    socket.addEventListener("message", (event) => {

        let data = JSON.parse(event.data);
        //calcular porcentaje
        let setpointPorcentage = data.setpoint * 2.77;
        let porcentage = data.measurement * 2.77;

        readout.textContent = data.measurement + " cm";
        liquid.style.height = porcentage + "%";
        setpoint.style.bottom = setpointPorcentage + "%";

        //graph the points 
        let now = Date.now();

        line1.append(now, data.measurement);

    });

    //graph setup
    let startTime = new Date().getTime();
    var smoothie = new SmoothieChart({
        minValue: 0,
        maxValue: 36,
        grid: {
            verticalSections: 6,
        },
        labels: {
            fillStyle: '#FFFFFF',
            precision: 0,
            showIntermediateLabels: true,
            intermediateLabelSameAxis: false,
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
    smoothie.streamTo(document.getElementById("chart"), 1000);

    let line1 = new TimeSeries();
    smoothie.addTimeSeries(line1, { strokeStyle: 'rgb(200,0,0)', lineWidth: 2 });

    //send data to esp32 
    range_setpoint.value = 0;
    setpoint_readout.textContent = range_setpoint.value;
    range_setpoint.addEventListener("input",()=>{
        setpoint_readout.textContent = range_setpoint.value;
    });
    
    range_setpoint.addEventListener("click", ()=>{
        setpoint_readout.textContent = range_setpoint.value;
        socket.send(JSON.stringify({setpoint: range_setpoint.value}));
    });

});
