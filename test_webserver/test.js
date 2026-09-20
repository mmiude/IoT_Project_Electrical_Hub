function update_device(device_id, values) {
    const assignments = [];

    if (values.name !== undefined) assignments.push(`name = ${values.name}`);
    if (values.priority !== undefined) assignments.push(`priority = ${values.priority}`);
    if (values.is_on !== undefined) assignments.push(`is_on = ${values.is_on}`);

    const s = `
        UPDATE device
        SET ${assignments.join(',\n            ')}
        WHERE id = ${device_id}
    `;
    console.log(s)
}

const values = [
{
    name: "test name" 
},
{
    priority: "LOW"
},
{
    is_on: true
},

{
    name: "test name",
    priority: "LOW"
},
{
    priority: "LOW",
    is_on: true
},
{
    name: "test name",
    is_on: true
},
{
    name: "test name",
    priority: "LOW",
    is_on: true
}
]

for (let i = 0; i < values.length; i++) {
    console.log("test " + i)
    update_device(123, values[i])
}