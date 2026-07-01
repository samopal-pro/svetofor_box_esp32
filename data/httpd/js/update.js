/** Загрузка и прошивка ESP32 */
async function upload(){
    const f=document.getElementById("fw").files[0];
    if(!f){alert("Не задан файл");return;}

    if(!f.name.endsWith(".bin")){
        alert("Файл должен быть .bin");
        return;
    }

    const form=new FormData();
    form.append("file",f);

    showMessage("Загрузка файла...");

    const r=await fetch("/update",{method:"POST",body:form});
//    checkCommand();
    showMessage(await r.text());
}  

function fileStyle(){
  const f=document.getElementById("fw");
  const n=document.getElementById("fileName");

  if(f){
    f.addEventListener("change",()=>{
      n.textContent = f.files.length ? f.files[0].name : "Файл не выбран";
    });
  }
}

