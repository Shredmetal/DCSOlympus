import * as L from 'leaflet'

export interface AirbaseOptions
{
    name: string,
    position: L.LatLng,
    src: string
}

export class Airbase extends L.Marker 
{
    #name: string = "";
    #coalition: string = "";
    #properties: string[] = [];
    #parkings: string[] = [];

    constructor(options: AirbaseOptions)
    {
        super(options.position, { riseOnHover: true });

        this.#name = options.name;
        var icon = new L.DivIcon({
            html: ` <div class="airbase" data-object="airbase" data-coalition="neutral">
                        <div class="airbase-marker"> </div>
                    </div>`, 
            className: 'leaflet-airbase-marker',
            iconSize: [40, 40],
            iconAnchor: [20, 20]
            });   // Set the marker, className must be set to avoid white square
        this.setIcon(icon); 
    }

    setCoalition(coalition: string)
    {
        this.#coalition = coalition;
        (<HTMLElement> this.getElement()?.querySelector(".airbase")).dataset.coalition = this.#coalition;
    }

    getCoalition()
    {
        return this.#coalition;
    }

    setName(name: string)
    {
        this.#name = name;
    }

    getName()
    {
        return this.#name;
    }

    setProperties(properties: string[])
    {
        this.#properties = properties;
    }

    getProperties()
    {
        return this.#properties;
    }

    setParkings(parkings: string[])
    {
        this.#parkings = parkings;
    }

    getParkings()
    {
        return this.#parkings;
    }
}