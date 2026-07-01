async function login()
{
    const pwd = document.getElementById('pwd').value;
    const msg = document.getElementById('msg');

    msg.textContent = '';

    try
    {
        const response = await fetch('/auth', {
            method: 'POST',
            headers: {
                'Content-Type':
                    'application/x-www-form-urlencoded'
            },
            body:
                'password=' +
                encodeURIComponent(pwd)
        });

        if (response.ok)
        {
            location.href = '/';
        }
        else
        {
            msg.textContent = 'Неверный пароль';
        }
    }
    catch(e)
    {
        msg.textContent = 'Ошибка соединения';
    }
}



function togglePassword()
{
    const pwd = document.getElementById('pwd');

    if (pwd.type === 'password')
    {
        pwd.type = 'text';
    }
    else
    {
        pwd.type = 'password';
    }
}