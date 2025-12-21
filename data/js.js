//first we have to create the object 
let smoothie = new SmoothieChart();
//communication with the canvas in the html
smoothie.streamTo(document.getElementById("chart"));

//each line requires a TimeSeries object

let liquid_height = new TimeSeries();

