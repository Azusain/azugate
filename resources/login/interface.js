function click_handler() {
    // window.location.href="info.html"
    console.log(    document.querySelector("input[placeholder=Username]") )
    console.log(    document.querySelector("input[placeholder=Password]") )

}

document.querySelector("button").addEventListener("click", click_handler)