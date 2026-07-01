async function loadConfig(schemaUrl) {
    const schema = await fetch(schemaUrl).then(r => r.json());

    const configDiv = document.getElementById('config');
    const buttonsDiv = document.getElementById('config-buttons');

    buttonsDiv.innerHTML = '';

    // создаём <p>, если его ещё нет
    let textP = configDiv.querySelector('p');
    if (!textP) {
        textP = document.createElement('p');
        configDiv.appendChild(textP);
    }

    // активная конфигурация
    const activeName = schema.active;

    if (schema[activeName]) {
        textP.textContent = schema[activeName].text || '';
    }

    // список конфигураций
    const configs = Object.keys(schema)
        .filter(k => k !== 'active' && typeof schema[k] === 'object');

    const table = document.createElement('table');
    table.style.width = '100%';

    const tr = document.createElement('tr');

    configs.forEach(key => {
        const cfg = schema[key];

        const td = document.createElement('td');
        td.style.width = (100 / configs.length) + '%';
        td.style.textAlign = 'center';

        const btn = document.createElement('input');
        btn.type = 'button';
        btn.name = cfg.name;
        btn.value = cfg.label;
        btn.style.width = '100px';
        if ( schema.active === cfg.name ){
            btn.style.background= "#88ff88";
        }

        btn.addEventListener('click', () => {
            sendConfig(cfg.name, cfg.label);
        });

        td.appendChild(btn);
        tr.appendChild(td);
    });

    table.appendChild(tr);
    buttonsDiv.appendChild(table);
}

function sendConfig(configName, label) {
    fetch('/cmd', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/x-www-form-urlencoded'
        },
        body: `cmd=config&config=${encodeURIComponent(configName)}`
    })
    .then(response => {
        if (!response.ok) {
            throw new Error(`HTTP error: ${response.status}`);
        }
        showMessage(`Установлена текущая конфигурация: \"${label}\". Сейчас траница перезагрузится`,'success');
        setTimeout(() => { location.reload();  }, 5000);
    })
    .catch(error => {
        showMessage(`Ошибка установки текущей конфигурации : ${error}`,'error');
    });
}