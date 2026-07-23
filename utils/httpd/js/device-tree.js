// Файл: httpd/js/device-tree.js
document.addEventListener('DOMContentLoaded', function() {
    let devices = [];
    let currentSort = { field: 'name', direction: 'asc' };
    
    loadDevices();
    
    function loadDevices() {
        const container = document.getElementById('device-list');
        container.innerHTML = '<div class="loading">Загрузка...</div>';
        
        fetch('/admin/config/devices.json')
            .then(response => {
                if (!response.ok) {
                    throw new Error('Ошибка загрузки данных');
                }
                return response.json();
            })
            .then(data => {
                devices = Object.entries(data).map(([name, info]) => ({
                    name: name,
                    serial: String(info.serial_no || '—'),
                    token: info.token || '',
                    configUuid: info.config_uuid || ''
                }));
                renderDevices();
            })
            .catch(error => {
                container.innerHTML = `<div class="error">Ошибка: ${error.message}</div>`;
            });
    }
    
    function sortDevices(field) {
        if (currentSort.field === field) {
            currentSort.direction = currentSort.direction === 'asc' ? 'desc' : 'asc';
        } else {
            currentSort.field = field;
            currentSort.direction = 'asc';
        }
        renderDevices();
    }
    
    function getSortValue(device, field) {
        switch(field) {
            case 'name':
                return String(device.name);
            case 'serial':
                return String(device.serial);
            case 'config':
                return device.configUuid && String(device.configUuid).trim() !== '' ? '1' : '0';
            default:
                return '';
        }
    }
    
    function renderDevices() {
        const container = document.getElementById('device-list');
        
        if (!devices.length) {
            container.innerHTML = '<div class="no-data">Нет данных об устройствах</div>';
            return;
        }
        
        const sorted = [...devices].sort((a, b) => {
            const valA = getSortValue(a, currentSort.field);
            const valB = getSortValue(b, currentSort.field);
            const result = String(valA).localeCompare(String(valB));
            return currentSort.direction === 'asc' ? result : -result;
        });
        
        const getArrow = (field) => {
            if (currentSort.field !== field) return '<span class="sort-arrow">⇅</span>';
            return currentSort.direction === 'asc' 
                ? '<span class="sort-arrow active">▲</span>' 
                : '<span class="sort-arrow active">▼</span>';
        };
        
        let html = '<table class="device-table">';
        html += '<thead><tr>';
        html += `<th onclick="window._sortDevices('name')">Устройство${getArrow('name')}</th>`;
        html += `<th onclick="window._sortDevices('serial')">Серийный номер${getArrow('serial')}</th>`;
        html += `<th onclick="window._sortDevices('config')">Конфигурация${getArrow('config')}</th>`;
        html += '</tr></thead><tbody>';
        
        sorted.forEach(device => {
            const hasConfig = device.configUuid && String(device.configUuid).trim() !== '';
            const nameClass = hasConfig ? 'device-name-cell has-config' : 'device-name-cell';
            
            html += '<tr>';
            html += `<td class="${nameClass}">${escapeHtml(device.name)}</td>`;
            html += `<td class="serial-cell">${escapeHtml(device.serial)}</td>`;
            
            if (hasConfig) {
                html += `<td><button class="config-link" data-device="${escapeAttr(device.name)}" onclick="window._openConfig(this, '${escapeAttr(device.name)}','${escapeAttr(device.serial)}','${escapeAttr(device.configUuid)}')">Открыть</button></td>`;
            } else {
                html += '<td><span class="config-none">нет конфигурации</span></td>';
            }
            
            html += '</tr>';
        });
        
        html += '</tbody></table>';
        container.innerHTML = html;
    }
    
    window._sortDevices = function(field) {
        sortDevices(field);
    };
    
    window._openConfig = function(btn, device_name, serial_no, config_uuid) {
        console.info(device_name, serial_no, config_uuid)
        btn.textContent = 'Загрузка...';
        btn.classList.add('loading');
        btn.disabled = true;
        
        fetch('/cmd', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/x-www-form-urlencoded',
            },
            body: `cmd=device&device_name=${encodeURIComponent(device_name)}&serial_no=${encodeURIComponent(serial_no)}&config_uuid=${encodeURIComponent(config_uuid)}`
        })
        .then(response => response.json())
        .then(data => {
            btn.textContent = 'Открыть';
            btn.classList.remove('loading');
            btn.disabled = false;
            
            const previewContainer = document.getElementById('preview-container');
            const previewIframe = document.getElementById('preview-iframe');
            const previewTitle = document.getElementById('preview-title');
            const listContainer = document.getElementById('list-container');
            
            previewTitle.textContent = device_name;
            previewIframe.src = '/';
            previewContainer.classList.add('active');
            listContainer.classList.add('with-preview');
        })
        .catch(error => {
            btn.textContent = 'Открыть';
            btn.classList.remove('loading');
            btn.disabled = false;
        });
    };
    
    window._closePreview = function() {
        const previewContainer = document.getElementById('preview-container');
        const previewIframe = document.getElementById('preview-iframe');
        const listContainer = document.getElementById('list-container');
        
        previewIframe.src = 'about:blank';
        previewContainer.classList.remove('active');
        listContainer.classList.remove('with-preview');
    };
    
    function escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = String(text);
        return div.innerHTML;
    }
    
    function escapeAttr(text) {
        return String(text).replace(/'/g, "\\'").replace(/"/g, '&quot;');
    }
});